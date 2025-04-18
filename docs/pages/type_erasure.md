# Type Erasure

Down with virtual methods.

## Traditional OOP

In normal C++, you would use a virtual interface to enable runtime polymorphism.

```cpp
class IDrawable {
public:
    virtual IDrawable() = 0;

    virtual void draw() = 0;
};

class Square : public IDrawable {
public:
    virtual void draw() override {
        std::println("Draw square.");
    }
};

class Circle : public IDrawable {
public:
    virtual void draw() override {
        std::println("Draw circle.");
    }
};
```

Now, to use this construction, you can pass a IDrawable by reference to a function. To actually store these things in a
memory safe way, either std::unique_ptr or std::shared_ptr must be used. These types cannot be treated as values, so we
must use indirection. Furthremore, the objects always have to be heap allocated.

## Using Type Erasure

Using type erasure, the interface class poses an abstract set of requirements, and can be constructed from any type
which meets them. The type internally is memory safe, by either storing the object internally (small object
optimization), or on the heap using a smart pointer. Instead of using 2 indirections, the virtual table can be inline or
otherwise stored using a "fat" pointer (rust dyn&).

```cpp
class Circle {};
class Sqaure {};

struct Draw {
    using Type = di::Method<Draw, void(di::This&)>;

    template<typename T>
    requires(di::concepts::TagInvocable<DrawFunction, T&>)
    void operator()(T& object) const {
        function::tag_invoke(*this, object);
    }
};

constexpr inline auto draw = detail::DrawFunction {};

using IDrawable = di::meta::List<Draw>;

using AnyDrawable = di::Any<IDrawable>;

void tag_invoke(Draw, Circle& self) {
    std::println("Draw circle.");
}

void tag_invoke(Draw, Square& self) {
    std::println("Draw square.");
}

void use_circle() {
    auto drawable = AnyDrawable(Circle {});
    draw(drawable);
}

void use_drawables() {
    auto drawables = std::vector<AnyDrawable> {};
    drawables.emplace_back(Circle {});
    drawables.emplace_back(Sqaure {});

    std::for_each(drawables, draw);
}
```

Notice, drawables can be used directly as objects, and thus don't have to managed using smart pointers. Additionally,
operations need not be defined inside the classes they operate on, which means Circle and Square can be pure data
classes, and offer no functionality themselves. This allows seamlessly adding new operations without breaking code.

Default operations can be expressed directly in the definition of the operation. By providing such a default operation,
the Draw function object will be invocable for any object, and thus every object can be erased into a drawable.

The tag_invoke mechanism allows type erasure without macros and without defining operations twice. However, you don't
get member functions, since C++ does not have reflection and meta classes. However, not using member functions allows
extending a class without modifying it, and still provides a uniform and readable way to call methods.

```cpp
// OOP
object->draw();

// Type erasure
draw(object);
```

Calling a free function is 2 characters shorter assuming we are already in the correct namespace, although otherwise it
will be more characters to type.

### Universality with Concepts

When using OOP, the dispatch is always dynamic, and so relies on de-virtualization to optimize scenarios where the
concrete object type is known. With type erasure, a concrete type and a polymorphic type can be used identically, and
static dispatch can be used by making your function generic with a template.

```cpp
// OOP: always use dynamic dispatch.
void draw3(IDrawable& a) {
    a.draw();
    a.draw();
    a.draw();
}

// Type erasure: maybe use dynamic dispatch, maybe not.
void draw3(di::Impl<IDrawable> auto& a) {
    draw(a);
    draw(a);
    draw(a);
}
```

`di::Impl` is a concept which is only satisfied for types which meet the requirements laid out in the provided
interface. The name is derived from rust, where traits can be required using the impl keyword.

## Ergonomic Concerns

The main annoyance with this model is the creation of tag_invoke calling function objects. This can actually be
automated using some meta programming on top of tag_invoke().

```cpp
// OOP
class IDrawable {
public:
    virtual ~IDrawable() = 0;

    virtual void draw() = 0;
    virtual i32 get_area() const = 0;
    virtual void debug_print() const {}
};

// Type erasure with method definition helper.
struct Draw : di::Dispatcher<Draw, void(di::This&)> {};
struct GetArea : di::Dispatcher<GetArea, i32(di::This const&)> {};
struct DebugPrint : di::Dispatcher<DebugPrint,
    void(di::This const&),
    di::Constexpr<di::into_void>
> {};

constexpr inline auto draw = Draw {};
constexpr inline auto get_area = GetArea {};
constexpr inline auto debug_print = DebugPrint {};

using IDrawable = di::meta::List<
    Draw, GetArea, DebugPrint
>;

using Drawable = di::Any<IDrawable>;
using DrawableRef = di::AnyRef<IDrawable>;
```

The idea is that the dispatch objects will implement the common CPO pattern, which is to attempt to call functions one
after another. For DebugPrint, the final function object to call is di::into_void, which means that the default
implenentation will just ignore arguments. This DSL for describing an interface can work without macros, and is in fact
far more expressive than virtual methods, since the actual method call can use static dispatch (and so can use if
constexpr).

### Templated Dispatch

Most type erased interfaces will be designed for type erasure, which means they will not have templated arguments. But,
in other cases, the interface in question will be templated. This is the case for things like sender/receivers, function
objects, di::format, etc. In these cases, an explicit signature must be provided. For di::format, a specific type erased
format context will be used, or for di::Function, the type erasure can only work on callables that except the provided
signature.

For these cases, there needs to be a way to explicitly list the signature when defining the interface requirements.

```cpp
using Interface = di::meta::List<
    Method1, Method2,
    di::Method<FunctionObject, void(di::This&)>,
    di::Method<OtherFunction, i32(di::This const&, i32)>
>;
```

If we really wanted, it could be possible to overload operator-> on some sort of type. The interface definition would
then be a list of value types:

```cpp
using Interface = di::meta::ValueList<
    method1, method2,
    di::member<FunctionObject(di::This&)> -> di::InPlaceType<void>,
    di::member<OtherFunction(di::This const&, i32)> -> di::InPlaceType<i32>
>;
```

This is more verbose and exotic, so probably won't be way to go.

### Expressivity for complex CPOs

The dispatcher API can also be used to make defining more complicated CPOs a lot more bearable.

The current implementation of container::begin() is as follows:

```cpp
struct BeginFunction;

namespace detail {
    template<typename T>
    concept ArrayBegin = concepts::LanguageArray<meta::RemoveReference<T>>;

    template<typename T>
    concept CustomBegin = concepts::TagInvocable<BeginFunction, T> &&
                          concepts::Iterator<meta::Decay<meta::TagInvokeResult<BeginFunction, T>>>;

    template<typename T>
    concept MemberBegin = requires(T&& container) {
                              { util::forward<T>(container).begin() } -> concepts::Iterator;
                          };
}

struct BeginFunction {
    template<typename T>
    requires(enable_borrowed_container(types::in_place_type<meta::RemoveCV<T>>) &&
             (detail::ArrayBegin<T> || detail::CustomBegin<T> || detail::MemberBegin<T>) )
    constexpr auto operator()(T&& container) const {
        if constexpr (detail::ArrayBegin<T>) {
            return container + 0;
        } else if constexpr (detail::CustomBegin<T>) {
            return function::tag_invoke(*this, util::forward<T>(container));
        } else {
            return util::forward<T>(container).begin();
        }
    }
};
```

This can equivalently be written as:

```cpp
namespace detail {
struct BeginArray {
    template<typename T>
    requires(concepts::LanguageArray<meta::RemoveReference<T>>)
    constexpr auto operator()(T&& array) const {
        return array + 0;
    }
};

struct BeginMember {
    template<typename T>
    constexpr auto operator()(T&& container) const
    requires(requires {
        { util::forward<T>(container).begin() } -> concepts::Iterator;
    })
    {
        return util::forward<T>(container).begin();
    }
};

struct BeginFunction : TemplateDipatcher<BeginFunction, meta::List<
    BeginArray(meta::_a),
    TagInvoke(This, meta::_a),
    BeginMemer(meta::_a)
>> {};
}
```

This makes use of a placeholder syntax to implicitly define template parameters of the function. This API is not yet
fully fleshed out.

### Method Resolution

The core machinery behind method lookup is the `di::Method` type. Instead of working on c++ functions, this works on
callable function objects and injects its own implementation using tag_invoke. This allows type-erasing simple
interfaces with no additional boilerplate.

However, this is not enough for more complicated interfaces. For example, consider the `di::clone()` function. This
function has a signature of `T clone(T const&)`, which is not a valid CPO signature. This is because the return type
varies with the input type. This is a problem for type erasure, because the return type must be known at compile time.

Conceptually, what we want to have happen is for the library to replace the return type `T` with the relevant `di::Any`
object which is being cloned. However, this means we cannot just call `di::clone` directly, because the return type may
not be implicitly convertible to `di::Any`.

To support this case, the type-erased function dispatch supports a second signature which is used for method lookup.
This signature takes the method as the second argument, which allows implementations to know what the type-erased return
type should be.

```cpp
using M = di::Method<di::Tag<di::clone>, di::This(di::This const&)>;

// When trying to resolve a call to M, first this signature is checked. Notice that the provided Method tag contains the
// concrete return type, which enables the custom implementation to make the necessary conversions.
tag_invoke(di::Tag<di::clone>, di::meta::MakeConcreteSignature<M, di::Any<...>> {}, di::This const&);

// Then the normal signature is checked, which will try to implicitly convert the return type to the correct di::Any.
di::clone(di::This const&);
```

## Multiple types of erased objects

The generic `di::Any` container supports both value and reference semantics, which can be thought of as equivalent to
passing a dyn& in rust. This type is 100% allocation free. This is controlled by the storage policy proivded to the
template, which allows `di::Any` to model shared ownership, unique (boxed) ownership, references, only inline storage,
hybrid storage, etc.

## Implementation

### Object Management

For the value oriented erased object, there are many considerations to be made, mainly around which operations are to be
supported. If a type erased wrapper supports copying, all implementations must support copying as well. The same can be
said for di CPOs, like di::clone. Making the erased object trivially relocatable greatly improves performance, because
indirect calls can be ellided during move construction, but this requires all implementing types to be trivially
relocatable themselves. This is mainly a problem because this information is not derived in the compiler (at least for
GCC), so such a property is opt-in.

No indirection on moving is needed if the internal object is always heap allocated, but doing so could be wasteful.
Having inline storage is very important when erasing small objects (imagine di::Function), but effectively useless if
every object is large (imagine iris::IrqController). As such, the internal storage policy needs to be heavily
customizable.

### Virtual Table Storage

Manually creating a vtable enables the programmer to micro-optimize the vtable layout as much as they please. A sensible
default is to store the vtable as a "fat" pointer (separate pointer to array of function pointers), but if there is only
1 operation, it is obviously better to just store that function pointer directly. Since we will always need at least 1
operation, because the destructor must always be callable, we can expand the default inlining level to 2 or 3
operations.

In certain cases, one function is "hot" while the other erased functions are called much less frequently. In these
scenarios, a hybrid approach should offer the best performance. This is again an area which requires extreme
customizability.

### Meta Object Representation

To store entries in the vtable, we need compile time meta programming facilities. Vtable entries will be represented in
the following structure.

```cpp
namespace types {
// Usage: Method<MyFunction, void(di::This&)>
template<typename T, concepts::LanguageFunction S>
struct Method {
    using Type = Method;
    using Tag = T;
    using Signature = S;
};
}

namespace concepts {
template<typename T>
concept Method = InstanceOf<T, types::Method>;
}

namespace meta {
template<concepts::Method Method>
using MethodTag = Method::Tag;

template<concepts::Method Method>
using MethodSignature = Method::Signature;
}
```

Then, vtables will have an associated list of signature objects, which correspond to the vtable entries. The library
will support merging vtables together, to enable erasing multiple traits into one object, and as an implementation
detail, because owning structures will internally merge the user requested operations with the vtable for moving,
destroying, copying, swapping, etc.

A list of methods will be represented directly use a `meta::List<>`. Thus, merging interfaces together is simply a
matter of calling `meta::Concat<>` and `meta::Unique<>` on all the methods present in a list.

### Object Categories

To enable certain optimizations when storing type erased objects, it is necessary to categorize object functionality.

| Category              | Requirements                                  | Optimization                                                                                     |
| --------------------- | --------------------------------------------- | ------------------------------------------------------------------------------------------------ |
| Reference             | Reference must remain valid (unsafe to store) | Only need sizeof(void\*) bytes of storage, no destruction, trivial copy, non-null                |
| Trivial               | Trivially Copyable, Destructible              | No need to erase lifetime operations (copy, move, destruct)                                      |
| Trivially Relocatable | Move = memcpy                                 | No need to erase move operations, but destructor is still required                               |
| Immovable             | Not copyable, movable                         | No need to erase move/copy, but objects cannot be copied/moved                                   |
| Move Only             | Movable                                       | No need to erase copy operations                                                                 |
| Copyable              | Movable and Copyable                          | Copying cannot return error, all lifetime operations cannot be merged into a single vtable entry |
| Infallibly cloneable  | Clone returns `T`, not `Result<T>`            | Clone function can never return an error                                                         |
| Cloneable             | Cloneable                                     | None                                                                                             |

Trivially relocatable objects provide a significant improvement over having a type erased move constructor. Nearly all
types in C++ are trivially relocatable, with the exception of linked lists (or any self-referential data structure). The
downside is that trivial relocatability is not tracked by the compiler, and so every type must manually opt-in.

The default object category will be move only, since this provides allow erasing nearly any object, while providing
normal value semantics. This also prevents expensive copy operations from ever being called.

If objects need to be copied, users can make the object category cloneable, in which case a vtable entry will handle
cloning, or if shared pointer semantics are required, users can use `AnyShared`, which internally stores a ref-counted
type-erased object.

### Any Type Summary

The following table describes the type aliases provided by the library. Notice that types which allocate memory take an
additional allocator template parameter, which defaults to `di::DefaultAllocator`.

| Name                                                                               | Alias                                                                             | Storage Category                 | Requirements on T                                           | Description                                                                                                                                                                  |
| ---------------------------------------------------------------------------------- | --------------------------------------------------------------------------------- | -------------------------------- | ----------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `di::Any<I>`                                                                       | `Any<I, HybridStorage<>`                                                          | Moveable                         | None                                                        | Owning polymorphic object with value semantics. This is the default storage and vtable policy used, and is good for storing any type of value.                               |
| `di::AnyRef<I>`                                                                    | `Any<I, RefStorage>`                                                              | Trivial                          | Must be reference or function pointer                       | Non-owning reference to polymorphic object. Unsafe to store, so only use when passing a parameter.                                                                           |
| `di::AnyInline<I, size, align>`                                                    | `Any<I, InlineStorage<size, align>>`                                              | Moveable                         | `sizeof(T) <= size` and `alignof(T) <= align` and Moveable  | Non-allocated owned storage. Use when the object sizes are guaranteed to be small or allocating is unacceptable, but note that objects which are too large cannot be stored. |
| `di::AnyUnique<I, Alloc = ...>`                                                    | `Any<I, UniqueStorage<Alloc>>`                                                    | Trivially Relocatable            | None                                                        | Always-allocated owned storage. Use when the object sizes are large or the move constructor needs to be called a lot.                                                        |
| `di::AnyHybrid<I, storage_category, size_threshold, align_threshold, Alloc = ...>` | `Any<I, HybridStorage<storage_category, size_threshold, align_threshold, Alloc>>` | Moveable                         | None (but T must be small and moveable to be stored inline) | Sometimes allocated owned storage. Use when the object size is unknown and can be small, which prevents allocating when storing some objects.                                |
| `di::AnyShared<I, Alloc = ...>`                                                    | `Any<I, SharedStorage<Alloc>>`                                                    | Trivially Relocatable + Copyable | None                                                        | Always-allocated shared storage. Use when shared ownership is required.                                                                                                      |

## A Practical Example

Consider a concept which current exists in di, which conceptifies any object which can have bytes written to. This
interface enables writing utility functions which work on anything which is byte writable, and in particular, is used by
di::format to allow printing to stdout and stderr. But, a Writer can also be implemented using memory mapped IO, or even
a temporary in memory buffer which has no disk backing.

The C++ 20 concept definition for this trait is as follows:

```cpp
template<typename T>
concept Writer = requires(T& writer, vocab::Span<Byte const> data) {
                     { writer.write_some(data) } -> SameAs<Result<usize>>;
                     { writer.flush() } -> SameAs<Result<void>>;
                 };
```

However, this trait definition requires any generic algorithm to be templated, and does not allow switching a Writer
implementation at runtime.

A type erased API definition looks like this:

```cpp
struct WriteSome : Dispatcher<WriteSome, Result<usize>(This&, Span<Byte const>)> {};
struct Flush : Dispatcher<Flush, Result<void>(This&)> {};

constexpr inline auto write_some = WriteSome {};
constexpr inline auto flush = Flush {};

using Writer = meta::List<WriteSome, Flush>;
```

Now imagine a BufferWriter class, which wraps any Writer and buffers repeated calls to write_some. This is done as
follows:

```cpp
// OLD: template<Writer W>
template<Impl<Writer> W>
class BufferWriter {
public:
    // OLD
    constexpr Result<usize> write_some(Span<Byte const> data) {
       // memcpy to buffer.
    }
    constexpr Result<void> flush() {
        DI_TRY(m_writer.write_some(/* ... */));
        return m_writer.flush();
    }

private:
    constexpr friend Result<usize> tag_invoke(WriteSome, BufferWriter& self, Span<Byte const>> data) {
        // memcpy to buffer.
    }

    constexpr friend Result<void> tag_invoke(Flush, BufferWriter& self) {
        DI_TRY(write_some(m_writer, /* ... */));
        return flush(m_writer);
    }

    W m_writer;
    Array<Byte, 4096> m_buffer;
};
```

By defining the writer concept as a type erasable interface, buffered writer can easily accept polymorphic types without
difficulty. This increased flexibility makes composition more powerful. Buffered writer is itself a Writer, so it too
can be erased into some polymorphic wrapper.

Interestingly, the member cased CPO mechanism is an instance of duck-typing, where it will accept anything which matches
the interface, even if the semantics are wrong. Where as the Any trait based solution requires specific opt-in to a
particular method, so a type can never be a Writer by accident.

As an additional bonus, extending the trait mechanism to include a default method implementation of `write_exactly()` is
trivial. But since this method has a default implementation, it cannot easily be extended to member function based
Writer. In practice, we would have to define `write_exactly` as a free function, and then users would have to know
whether the method they wish to call is a free function or a member function. An alternative is to use CRTP, where every
concrete writer class must inherit from `WriterInterface<Self>`. This does in fact work, but results in more code than
the trait solution, while also not easily allowing polymorphic value types.
