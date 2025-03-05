# Bytes

## Purpose

Most low-level code has to deal directly with bytes. This can be for reading and parsing various file formats,
or writing data to a frame buffer or physical device. Because C++ does not have a borrow checker, these interfaces
typically need to be concerned with the lifetime of the bytes they are operating on. The library aims to solve this
problem by providing a generic (over storage method) byte buffer mechanism. Additionally, there are facilities to
perform bit-level and byte-level IO asynchronously, while even allowing for zero-copy operations.

## Byte Buffer

In certain scenarios, like creating audio data, code needs to actually write directly to the backing bytes. However,
most of the time, code is only interested in reading the data. These 2 different scenarios represent exclusive
references and shared references respectively (in Rust). Both types of references can actually use the same type-erased
backing store.

### Type Erased Backing Store

Any class which aims to represent a series of contiguous bytes can be represented using only 2 member variables:

1. `byte*` data
2. `usize` size

Unlike a normal type-erased class, where we would need to perform a indirect (virtual) call in order to access the
concrete type, we can instead store these member variables directly (as a `di::Span`) on object construction. This
means that the only member variable which needs to be type-erased are the special member functions. As a further
optimization, we can require the backing store be trivially relocatable to allow move construction to not require
an indirect call.

The backing store now looks something like this:

```cpp
struct AnyBytesStorage {
    di::Any</* ... */> object;
    di::Span<byte> data;
};
```

### Shared Ownership

Shared ownership requires not giving mutable access to the underlying bytes (we should return a `byte const*` for the
data). This is to prevent data races. The underlying backing memory can either be reference-counted or statically
allocated, since the backing object is type-erased. Similarly, the object's destructor can do anything, such as calling
`operator delete[]`, `munmap`, `smh_unlink`, and so on. This enables zero-copy de-serialization.

For instance, a parser for the WAV file format usually doesn't have to do much. This file format essentially just
stores raw audio samples on disk (in most cases). Rather than copying the data into our own audio buffer, we can
now directly reference the file's memory in a safe way, by references a backing store which will eventually call
`munmap()`.

A key reason this is possible is that we can shrink the shared bye buffer at any time, while still keeping a reference
to the original object. This is because the actual span of bytes is separate from the underlying object. This ability
is similar to `std::shared_ptr`.

### Unique Ownership

A unique byte buffer can use the same backing store as the shared variant, but will allow getting a mutable reference
to the underlying bytes. The key point here is that once we have wrote data into the buffer, we can then safely share
the data by converting ourselves into a shared reference. This conversion only works 1 way, it is not safe to convert
a shared reference into an owning one.
