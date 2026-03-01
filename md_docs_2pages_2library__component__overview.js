var md_docs_2pages_2library__component__overview =
[
    [ "Overview", "md_docs_2pages_2library__component__overview.html#autotoc_md89", null ],
    [ "Table of Contents", "md_docs_2pages_2library__component__overview.html#autotoc_md90", null ],
    [ "Allocator", "md_docs_2pages_2allocator.html", [
      [ "Purpose", "md_docs_2pages_2allocator.html#autotoc_md6", null ],
      [ "Problem with Standard C++ Allocators", "md_docs_2pages_2allocator.html#autotoc_md7", null ],
      [ "Allocator Interface", "md_docs_2pages_2allocator.html#autotoc_md8", null ],
      [ "Allocator Usage", "md_docs_2pages_2allocator.html#autotoc_md9", null ],
      [ "Allocator Interaction with Containers", "md_docs_2pages_2allocator.html#autotoc_md10", null ],
      [ "Conceptual Example", "md_docs_2pages_2allocator.html#autotoc_md11", null ]
    ] ],
    [ "Bytes", "md_docs_2pages_2bytes.html", [
      [ "Purpose", "md_docs_2pages_2bytes.html#autotoc_md24", null ],
      [ "Byte Buffer", "md_docs_2pages_2bytes.html#autotoc_md25", [
        [ "Type Erased Backing Store", "md_docs_2pages_2bytes.html#autotoc_md26", null ],
        [ "Shared Ownership", "md_docs_2pages_2bytes.html#autotoc_md27", null ],
        [ "Unique Ownership", "md_docs_2pages_2bytes.html#autotoc_md28", null ]
      ] ]
    ] ],
    [ "Execution", "md_docs_2pages_2execution.html", [
      [ "Purpose", "md_docs_2pages_2execution.html#autotoc_md40", null ],
      [ "Conceptual Overview", "md_docs_2pages_2execution.html#autotoc_md41", [
        [ "Life Time Model", "md_docs_2pages_2execution.html#autotoc_md42", null ]
      ] ],
      [ "Async Sequences", "md_docs_2pages_2execution.html#autotoc_md43", [
        [ "Who calls set_next()?", "md_docs_2pages_2execution.html#autotoc_md44", null ],
        [ "set_next() Allows the Receiver to Communicate Back to the Sequence", "md_docs_2pages_2execution.html#autotoc_md45", null ],
        [ "Async Sequence Life Time Model", "md_docs_2pages_2execution.html#autotoc_md46", null ],
        [ "Lockstep Sequences", "md_docs_2pages_2execution.html#autotoc_md47", null ],
        [ "How do completion signatures work with sequences?", "md_docs_2pages_2execution.html#autotoc_md48", null ],
        [ "Comparison with libunifex Models", "md_docs_2pages_2execution.html#autotoc_md49", null ],
        [ "Drawbacks of the Async Sequence Model", "md_docs_2pages_2execution.html#autotoc_md50", null ]
      ] ],
      [ "Async RAII", "md_docs_2pages_2execution.html#autotoc_md51", [
        [ "The Async Call Stack", "md_docs_2pages_2execution.html#autotoc_md52", null ],
        [ "Cleanup can be Asynchronous", "md_docs_2pages_2execution.html#autotoc_md53", null ],
        [ "Cleanup can be Fallible", "md_docs_2pages_2execution.html#autotoc_md54", null ],
        [ "Async RAII Working Design", "md_docs_2pages_2execution.html#autotoc_md55", null ],
        [ "use_resources() Implementation", "md_docs_2pages_2execution.html#autotoc_md56", null ],
        [ "make_deferred Implementation", "md_docs_2pages_2execution.html#autotoc_md57", null ],
        [ "Async RAII in Coroutines", "md_docs_2pages_2execution.html#autotoc_md58", null ]
      ] ],
      [ "Async Scope", "md_docs_2pages_2execution.html#autotoc_md59", [
        [ "Nest", "md_docs_2pages_2execution.html#autotoc_md60", null ],
        [ "Spawn", "md_docs_2pages_2execution.html#autotoc_md61", null ],
        [ "Spawn Future", "md_docs_2pages_2execution.html#autotoc_md62", null ],
        [ "Counting Scope", "md_docs_2pages_2execution.html#autotoc_md63", null ],
        [ "Benefits of Async Scope Abstraction", "md_docs_2pages_2execution.html#autotoc_md64", null ]
      ] ],
      [ "Type Erased Sender", "md_docs_2pages_2execution.html#autotoc_md65", [
        [ "How does this work?", "md_docs_2pages_2execution.html#autotoc_md66", null ],
        [ "Problems with this Approach", "md_docs_2pages_2execution.html#autotoc_md67", [
          [ "Case 1: Creating the Type-Erased Receiver Fails", "md_docs_2pages_2execution.html#autotoc_md68", null ],
          [ "Case 2: Creating the Type-Erased Sender Fails", "md_docs_2pages_2execution.html#autotoc_md69", null ],
          [ "Case 3: Creating the Type-Erased Operation State Fails", "md_docs_2pages_2execution.html#autotoc_md70", null ]
        ] ]
      ] ],
      [ "References", "md_docs_2pages_2execution.html#autotoc_md71", null ]
    ] ],
    [ "Intrusive Containers", "md_docs_2pages_2intrusive.html", [
      [ "Comparison with Owning Containers", "md_docs_2pages_2intrusive.html#autotoc_md73", null ],
      [ "Main Concern with Intrusive Containers", "md_docs_2pages_2intrusive.html#autotoc_md74", null ],
      [ "Intrusive Container Customizations", "md_docs_2pages_2intrusive.html#autotoc_md75", null ],
      [ "Using the IntrusiveList class", "md_docs_2pages_2intrusive.html#autotoc_md76", null ]
    ] ],
    [ "Inter-Process Communication", "md_docs_2pages_2ipc.html", [
      [ "Purpose", "md_docs_2pages_2ipc.html#autotoc_md78", null ],
      [ "Conceptual Overview", "md_docs_2pages_2ipc.html#autotoc_md79", [
        [ "Sending Messages", "md_docs_2pages_2ipc.html#autotoc_md80", null ],
        [ "Receiving Messages", "md_docs_2pages_2ipc.html#autotoc_md81", null ],
        [ "Sending and Receiving Messages", "md_docs_2pages_2ipc.html#autotoc_md82", null ],
        [ "Connection Management", "md_docs_2pages_2ipc.html#autotoc_md83", null ]
      ] ],
      [ "Usage", "md_docs_2pages_2ipc.html#autotoc_md84", [
        [ "Defining a Message Type", "md_docs_2pages_2ipc.html#autotoc_md85", null ],
        [ "Creating a Connection", "md_docs_2pages_2ipc.html#autotoc_md86", null ]
      ] ],
      [ "Synchronization", "md_docs_2pages_2ipc.html#autotoc_md87", null ]
    ] ],
    [ "Serialization", "md_docs_2pages_2serialization.html", [
      [ "Purpose", "md_docs_2pages_2serialization.html#autotoc_md92", null ],
      [ "Usage", "md_docs_2pages_2serialization.html#autotoc_md93", null ],
      [ "Custom Serialization Formats", "md_docs_2pages_2serialization.html#autotoc_md94", null ],
      [ "Custom Deserialization Formats", "md_docs_2pages_2serialization.html#autotoc_md95", null ]
    ] ],
    [ "Static Reflection", "md_docs_2pages_2static__reflection.html", [
      [ "Purpose", "md_docs_2pages_2static__reflection.html#autotoc_md97", null ],
      [ "Note on C++", "md_docs_2pages_2static__reflection.html#autotoc_md98", null ],
      [ "Usage", "md_docs_2pages_2static__reflection.html#autotoc_md99", null ],
      [ "Internal Representation", "md_docs_2pages_2static__reflection.html#autotoc_md100", [
        [ "Atoms", "md_docs_2pages_2static__reflection.html#autotoc_md101", null ]
      ] ],
      [ "Accessing Reflection Information", "md_docs_2pages_2static__reflection.html#autotoc_md102", null ],
      [ "Uses in library", "md_docs_2pages_2static__reflection.html#autotoc_md103", null ],
      [ "Limitations", "md_docs_2pages_2static__reflection.html#autotoc_md104", null ]
    ] ],
    [ "Type Erasure", "md_docs_2pages_2type__erasure.html", [
      [ "Traditional OOP", "md_docs_2pages_2type__erasure.html#autotoc_md106", null ],
      [ "Using Type Erasure", "md_docs_2pages_2type__erasure.html#autotoc_md107", [
        [ "Universality with Concepts", "md_docs_2pages_2type__erasure.html#autotoc_md108", null ]
      ] ],
      [ "Ergonomic Concerns", "md_docs_2pages_2type__erasure.html#autotoc_md109", [
        [ "Templated Dispatch", "md_docs_2pages_2type__erasure.html#autotoc_md110", null ],
        [ "Expressivity for complex CPOs", "md_docs_2pages_2type__erasure.html#autotoc_md111", null ],
        [ "Method Resolution", "md_docs_2pages_2type__erasure.html#autotoc_md112", null ]
      ] ],
      [ "Multiple types of erased objects", "md_docs_2pages_2type__erasure.html#autotoc_md113", null ],
      [ "Implementation", "md_docs_2pages_2type__erasure.html#autotoc_md114", [
        [ "Object Management", "md_docs_2pages_2type__erasure.html#autotoc_md115", null ],
        [ "Virtual Table Storage", "md_docs_2pages_2type__erasure.html#autotoc_md116", null ],
        [ "Meta Object Representation", "md_docs_2pages_2type__erasure.html#autotoc_md117", null ],
        [ "Object Categories", "md_docs_2pages_2type__erasure.html#autotoc_md118", null ],
        [ "Any Type Summary", "md_docs_2pages_2type__erasure.html#autotoc_md119", null ]
      ] ],
      [ "A Practical Example", "md_docs_2pages_2type__erasure.html#autotoc_md120", null ]
    ] ]
];