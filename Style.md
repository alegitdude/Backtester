From Google's CppGuide

    Declaring Order:
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