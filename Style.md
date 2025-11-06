From Google's CppGuide
    <!-- #region -->  Class Format:

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

    <!-- #region -->  Declaring Order:
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

     <!-- #region --> Function Names:
        Regular functions have mixed case; accessors and mutators may be named like variables.

        Ordinarily, functions should start with a capital letter and have a capital letter for each new word.

        AddTableEntry()
        DeleteUrl()
        OpenFileOrDie()

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

    <!-- #region -->  Variable Names

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
    
    <!-- #region -->  Constant Names:

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

    <!-- #region -->  Enumerator Names

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
<!-- #region -->   Function Declarations and Definitions
    Return type on the same line as function name, parameters on the same line if they fit. Wrap parameter lists which do not fit on a single line as you would wrap arguments in a function call.

    Functions look like this:

    ReturnType ClassName::FunctionName(Type par_name1, Type par_name2) {
    DoSomething();
    ...
    }
    If you have too much text to fit on one line:

    ReturnType ClassName::ReallyLongFunctionName(Type par_name1, Type par_name2,
                                                Type par_name3) {
    DoSomething();
    ...
    }
    or if you cannot fit even the first parameter:

    ReturnType LongClassName::ReallyReallyReallyLongFunctionName(
        Type par_name1,  // 4 space indent
        Type par_name2,
        Type par_name3) {
    DoSomething();  // 2 space indent
    ...
    }
    Some points to note:

    Choose good parameter names.
    A parameter name may be omitted only if the parameter is not used in the function's definition.
    If you cannot fit the return type and the function name on a single line, break between them.
    If you break after the return type of a function declaration or definition, do not indent.
    The open parenthesis is always on the same line as the function name.
    There is never a space between the function name and the open parenthesis.
    There is never a space between the parentheses and the parameters.
    The open curly brace is always on the end of the last line of the function declaration, not the start of the next line.
    The close curly brace is either on the last line by itself or on the same line as the open curly brace.
    There should be a space between the close parenthesis and the open curly brace.
    All parameters should be aligned if possible.
    Default indentation is 2 spaces.
    Wrapped parameters have a 4 space indent.
    Unused parameters that are obvious from context may omit the name:

    class Foo {
    public:
    Foo(const Foo&) = delete;
    Foo& operator=(const Foo&) = delete;
    };
    Unused parameters that might not be obvious should comment out the variable name in the function definition:

    class Shape {
    public:
    virtual void Rotate(double radians) = 0;
    };

    class Circle : public Shape {
    public:
    void Rotate(double radians) override;
    };

    void Circle::Rotate(double /*radians*/) {}
    // Bad - if someone wants to implement later, it's not clear what the
    // variable means.
    void Circle::Rotate(double) {}
    Attributes, and macros that expand to attributes, appear at the very beginning of the function declaration or definition, before the return type:

    ABSL_ATTRIBUTE_NOINLINE void ExpensiveFunction();
    [[nodiscard]] bool IsOk();

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
    <!-- #region -->  Inheritance

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

    <!-- #region -->  Ownership Smart Pointers

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


 <!-- #region --> Exceptions

Exceptions

We do not use C++ exceptions.

    Exceptions allow higher levels of an application to decide how to handle "can't happen" failures in deeply nested functions, without the obscuring and error-prone bookkeeping of error codes.
    Exceptions are used by most other modern languages. Using them in C++ would make it more consistent with Python, Java, and the C++ that others are familiar with.
    Some third-party C++ libraries use exceptions, and turning them off internally makes it harder to integrate with those libraries.
    Exceptions are the only way for a constructor to fail. We can simulate this with a factory function or an Init() method, but these require heap allocation or a new "invalid" state, respectively.
    Exceptions are really handy in testing frameworks.

    When you add a throw statement to an existing function, you must examine all of its transitive callers. Either they must make at least the basic exception safety guarantee, or they must never catch the exception and be happy with the program terminating as a result. For instance, if f() calls g() calls h(), and h throws an exception that f catches, g has to be careful or it may not clean up properly.
    More generally, exceptions make the control flow of programs difficult to evaluate by looking at code: functions may return in places you don't expect. This causes maintainability and debugging difficulties. You can minimize this cost via some rules on how and where exceptions can be used, but at the cost of more that a developer needs to know and understand.
    Exception safety requires both RAII and different coding practices. Lots of supporting machinery is needed to make writing correct exception-safe code easy. Further, to avoid requiring readers to understand the entire call graph, exception-safe code must isolate logic that writes to persistent state into a "commit" phase. This will have both benefits and costs (perhaps where you're forced to obfuscate code to isolate the commit). Allowing exceptions would force us to always pay those costs even when they're not worth it.
    Turning on exceptions adds data to each binary produced, increasing compile time (probably slightly) and possibly increasing address space pressure.
    The availability of exceptions may encourage developers to throw them when they are not appropriate or recover from them when it's not safe to do so. For example, invalid user input should not cause exceptions to be thrown. We would need to make the style guide even longer to document these restrictions!

On their face, the benefits of using exceptions outweigh the costs, especially in new projects. However, for existing code, the introduction of exceptions has implications on all dependent code. If exceptions can be propagated beyond a new project, it also becomes problematic to integrate the new project into existing exception-free code. Because most existing C++ code at Google is not prepared to deal with exceptions, it is comparatively difficult to adopt new code that generates exceptions.

Given that Google's existing code is not exception-tolerant, the costs of using exceptions are somewhat greater than the costs in a new project. The conversion process would be slow and error-prone. We don't believe that the available alternatives to exceptions, such as error codes and assertions, introduce a significant burden.

Our advice against using exceptions is not predicated on philosophical or moral grounds, but practical ones. Because we'd like to use our open-source projects at Google and it's difficult to do so if those projects use exceptions, we need to advise against exceptions in Google open-source projects as well. Things would probably be different if we had to do it all over again from scratch.

This prohibition also applies to exception handling related features such as std::exception_ptr and std::nested_exception.

There is an exception to this rule (no pun intended) for Windows code.



 
Type	 Bits (Width) Memory (Bytes) Minimum Value	Maximum Value
uint16_t  16 bits	  2 bytes	         0	    65,535
uint32_t  32 bits	  4 bytes	0	    4,294,967,295
uint64_t  64 bits	  8 bytes	0	    18,446,744,073,709,551,615
uint8_t	  8 bits	  1 byte	0	    255