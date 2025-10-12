From Google's CppGuide
    - Class Format:

    Sections in public, protected and private order, each indented one space.

    The basic format for a class definition (lacking the comments, see Class Comments for a discussion of what comments are needed) is:

    class MyClass : public OtherClass {
     public:      // Note the 1 space indent!
      MyClass();  // Regular 2 space indent.
      explicit MyClass(int var);
      ~MyClass() {}

      void SomeFunction();
      void SomeFunctionThatDoesNothing() {
      }

      void set_some_var(int var) { some_var_ = var; }
      int some_var() const { return some_var_; }

     private:
      bool SomeInternalFunction();

      int some_var_;
      int some_other_var_;
    };

    Things to note:

        Any base class name should be on the same line as the subclass name, subject to the 80-column limit.
        The public:, protected:, and private: keywords should be indented one space.
        Except for the first instance, these keywords should be preceded by a blank line. This rule is optional in small classes.
        Do not leave a blank line after these keywords.
        The public section should be first, followed by the protected and finally the private section.
        See Declaration Order for rules on ordering declarations within each of these sections.

    Constructor Initializer Lists

    Constructor initializer lists can be all on one line or with subsequent lines indented four spaces.

    The acceptable formats for initializer lists are:

    // When everything fits on one line:
    MyClass::MyClass(int var) : some_var_(var) {
    DoSomething();
    }

    // If the signature and initializer list are not all on one line,
    // you must wrap before the colon and indent 4 spaces:
    MyClass::MyClass(int var)
        : some_var_(var), some_other_var_(var + 1) {
    DoSomething();
    }

    // When the list spans multiple lines, put each member on its own line
    // and align them:
    MyClass::MyClass(int var)
        : some_var_(var),             // 4 space indent
        some_other_var_(var + 1) {  // lined up
    DoSomething();
    }

    // As with any other code block, the close curly can be on the same
    // line as the open curly, if it fits.
    MyClass::MyClass(int var)
        : some_var_(var) {}
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

    - Declaring Order:
        Group similar declarations together, placing public parts earlier.

        A class definition should usually start with a public: section, followed by protected:, then private:. Omit sections that would be empty.

        Within each section, prefer grouping similar kinds of declarations together, and prefer the following order:

            Types and type aliases (typedef, using, enum, nested structs and classes, and friend types)
            (Optionally, for structs only) non-static data members
            Static constants
            Factory functions
            Constructors and assignment operators
            Destructor
            All other functions (static and non-static member functions, and friend functions)
            All other data members (static and non-static)
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

    - Function Names:
        Regular functions have mixed case; accessors and mutators may be named like variables.

        Ordinarily, functions should start with a capital letter and have a capital letter for each new word.

        AddTableEntry()
        DeleteUrl()
        OpenFileOrDie()

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

    - Variable Names

    The names of variables (including function parameters) and data members are snake_case (all lowercase, with underscores between words). Data members of classes (but not structs) additionally have trailing underscores. For instance: a_local_variable, a_struct_data_member, a_class_data_member_.

        Common Variable names

    For example:

        std::string table_name;  // OK - snake_case.

        std::string tableName;   // Bad - mixed case.

    - Class Data Members

    Data members of classes, both static and non-static, are named like ordinary nonmember variables, but with a trailing underscore. The exception to this is static constant class members, which should follow the rules for naming constants.

        class TableInfo {
        public:
        ...
        static const int kTableVersion = 3;  // OK - constant naming.
        ...

        private:
        std::string table_name_;             // OK - underscore at end.
        static Pool<TableInfo>* pool_;       // OK.
        };

    Struct Data Members

    Data members of structs, both static and non-static, are named like ordinary nonmember variables. They do not have the trailing underscores that data members in classes have.

    struct UrlTableProperties {
    std::string name;
    int num_entries;
    static Pool<UrlTableProperties>* pool;
    };

    See Structs vs. Classes for a discussion of when to use a struct versus a class.

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
    
    - Constant Names:

        Variables declared constexpr or const, and whose value is fixed for the duration of the program, are named with a leading "k" followed by mixed case. Underscores can be used as separators in the rare cases where capitalization cannot be used for separation. For example:

        const int kDaysInAWeek = 7;
        const int kAndroid8_0_0 = 24;  // Android 8.0.0

        All such variables with static storage duration (i.e., statics and globals, see Storage Duration for details) should be named this way, including those that are static constant class data members and those in templates where different instantiations of the template may have different values. This convention is optional for variables of other storage classes, e.g., automatic variables; otherwise the usual variable naming rules apply. For example:

        void ComputeFoo(absl::string_view suffix) {
        // Either of these is acceptable.
        const absl::string_view kPrefix = "prefix";
        const absl::string_view prefix = "prefix";
        ...
        }

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

    - Enumerator Names

        Enumerators (for both scoped and unscoped enums) should be named like constants, not like macros. That is, use kEnumName not ENUM_NAME.

        enum class UrlTableError {
        kOk = 0,
        kOutOfMemory,
        kMalformedInput,
        };

        enum class AlternateUrlTableError {
        OK = 0,
        OUT_OF_MEMORY = 1,
        MALFORMED_INPUT = 2,
        };

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

    - Inheritance

        Composition is often more appropriate than inheritance. When using inheritance, make it public.

        When a sub-class inherits from a base class, it includes the definitions of all the data and operations that the base class defines. "Interface inheritance" is inheritance from a pure abstract base class (one with no state or defined methods); all other inheritance is "implementation inheritance".

        Implementation inheritance reduces code size by re-using the base class code as it specializes an existing type. Because inheritance is a compile-time declaration, you and the compiler can understand the operation and detect errors. Interface inheritance can be used to programmatically enforce that a class expose a particular API. Again, the compiler can detect errors, in this case, when a class does not define a necessary method of the API.

        For implementation inheritance, because the code implementing a sub-class is spread between the base and the sub-class, it can be more difficult to understand an implementation. The sub-class cannot override functions that are not virtual, so the sub-class cannot change implementation.

        Multiple inheritance is especially problematic, because it often imposes a higher performance overhead (in fact, the performance drop from single inheritance to multiple inheritance can often be greater than the performance drop from ordinary to virtual dispatch), and because it risks leading to "diamond" inheritance patterns, which are prone to ambiguity, confusion, and outright bugs.

        All inheritance should be public. If you want to do private inheritance, you should be including an instance of the base class as a member instead. You may use final on classes when you don't intend to support using them as base classes.

        Do not overuse implementation inheritance. Composition is often more appropriate. Try to restrict use of inheritance to the "is-a" case: Bar subclasses Foo if it can reasonably be said that Bar "is a kind of" Foo.

        Limit the use of protected to those member functions that might need to be accessed from subclasses. Note that data members should be private.

        Explicitly annotate overrides of virtual functions or virtual destructors with exactly one of an override or (less frequently) final specifier. Do not use virtual when declaring an override. Rationale: A function or destructor marked override or final that is not an override of a base class virtual function will not compile, and this helps catch common errors. The specifiers serve as documentation; if no specifier is present, the reader has to check all ancestors of the class in question to determine if the function or destructor is virtual or not.

        Multiple inheritance is permitted, but multiple implementation inheritance is strongly discouraged.

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

    - Ownership and Smart Pointers

        Prefer to have single, fixed owners for dynamically allocated objects. Prefer to transfer ownership with smart pointers.

        "Ownership" is a bookkeeping technique for managing dynamically allocated memory (and other resources). The owner of a dynamically allocated object is an object or function that is responsible for ensuring that it is deleted when no longer needed. Ownership can sometimes be shared, in which case the last owner is typically responsible for deleting it. Even when ownership is not shared, it can be transferred from one piece of code to another.

        "Smart" pointers are classes that act like pointers, e.g., by overloading the * and -> operators. Some smart pointer types can be used to automate ownership bookkeeping, to ensure these responsibilities are met. std::unique_ptr is a smart pointer type which expresses exclusive ownership of a dynamically allocated object; the object is deleted when the std::unique_ptr goes out of scope. It cannot be copied, but can be moved to represent ownership transfer. std::shared_ptr is a smart pointer type that expresses shared ownership of a dynamically allocated object. std::shared_ptrs can be copied; ownership of the object is shared among all copies, and the object is deleted when the last std::shared_ptr is destroyed.

            It's virtually impossible to manage dynamically allocated memory without some sort of ownership logic.
            Transferring ownership of an object can be cheaper than copying it (if copying it is even possible).
            Transferring ownership can be simpler than 'borrowing' a pointer or reference, because it reduces the need to coordinate the lifetime of the object between the two users.
            Smart pointers can improve readability by making ownership logic explicit, self-documenting, and unambiguous.
            Smart pointers can eliminate manual ownership bookkeeping, simplifying the code and ruling out large classes of errors.
            For const objects, shared ownership can be a simple and efficient alternative to deep copying.

            Ownership must be represented and transferred via pointers (whether smart or plain). Pointer semantics are more complicated than value semantics, especially in APIs: you have to worry not just about ownership, but also aliasing, lifetime, and mutability, among other issues.
            The performance costs of value semantics are often overestimated, so the performance benefits of ownership transfer might not justify the readability and complexity costs.
            APIs that transfer ownership force their clients into a single memory management model.
            Code using smart pointers is less explicit about where the resource releases take place.
            std::unique_ptr expresses ownership transfer using move semantics, which are relatively new and may confuse some programmers.
            Shared ownership can be a tempting alternative to careful ownership design, obfuscating the design of a system.
            Shared ownership requires explicit bookkeeping at run-time, which can be costly.
            In some cases (e.g., cyclic references), objects with shared ownership may never be deleted.
            Smart pointers are not perfect substitutes for plain pointers.

        If dynamic allocation is necessary, prefer to keep ownership with the code that allocated it. If other code needs access to the object, consider passing it a copy, or passing a pointer or reference without transferring ownership. Prefer to use std::unique_ptr to make ownership transfer explicit. For example:

        std::unique_ptr<Foo> FooFactory();
        void FooConsumer(std::unique_ptr<Foo> ptr);

        Do not design your code to use shared ownership without a very good reason. One such reason is to avoid expensive copy operations, but you should only do this if the performance benefits are significant, and the underlying object is immutable (i.e., std::shared_ptr<const Foo>). If you do use shared ownership, prefer to use std::shared_ptr.

        Never use std::auto_ptr. Instead, use std::unique_ptr.
