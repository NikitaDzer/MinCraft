# C/C++ code style
## Table of content
1. [Requirements](#requirements)
2. [List of rules](#list-of-rules)
3. [File naming convention ](#file-naming-convention)
4. [Header include](#header-include)
5. [Header file structure](#header-file-structure)
6. [Source file structure](#source-file-structure)
7. [Function declaration](#function-declaration)
8. [Function definition](#function-definition)
9. [Space and tabs](#space-and-tabs)
10. [Control statements](#control-statements)
11. [Naming rules](#naming-rules)

## Requirements
Anything in the project should be written in one style that stated in this document.
n describing the style, the rules are defined, which must be followed at all times, and the recommendations from which deviations must be under special control and in a minimum number.

## List of rules
### File naming convention
All the file names should use snake case, for example *some_file.cc*
Agreements for using suffixes in filenames:
* .c - C source file,
* .cc - C++ source file,
* .h - C/C++ header file.
### Header include
Every header file should be protected from double include using #pragma once:

```cpp
#pragma once

/**
 * File content.
 */
```

### Header file structure
* Prologue, which contains a comment to the whole file with a description of the functional area of the component.
* Include of non-standard headers and *after* it include of standard headers (stdio.h, etc...) .
* Enums.
* Constant with extern qulificator.
* Non-extern global constant. (should be commented the reason of usage)
* Class declaration.
* Macros description.
* Extern functions.
* Non-extern function declarations.

### Source file structure
* The structure is same as header file structure but source file contain definition of non-extern functions and class function members.

### Function declaration
Function declaration should look like this and placed in header file:

```cpp
extern void 
funcName( int first_parameter,
          long second_parameter,
          char* p_third_parameter );
```

Member function declaration should look like this:
```cpp
class SomeUsefulClass
{
    void someFunctionMember( int first_parameter,
                             long second_parameter );
};
```
### Function definition
* Leave comment with brief function description.
* Leave comment next to each parameter.
* Leave function name in comment after definition.

Function definition should look like this and placed in source file:
```cpp
/*
 * Brief function description.
 */
static void
funcName( int first_parameter,      /* comment for first parameter */
          long second_parameter,    /* comment for second parameter */
          char* p_third_parameter ) /* comment for third parameter */
{
    printf( "%d %ld %s\\n",
            first_parameter, second_parameter, p_third_parameter );
} // funcName
```

For member function definition code style is different.
* Rules for comments are optional.
* Leave return-type on line with function name.
* Small member functions may be written in one line.

Member function definition should look like this:
```cpp
class SomeUsefulClass
{
    void someFunctionMember( int first_parameter,
                             long second_parameter )
    {
        // Your code
    }

    void smallFunctionMember( int parameter ) { /* Your code */ }
};
```

### Space and tabs
Tabs should be replaced by spaces. Tab width should be 4.

### Control statements
Control statement parentheses should be spaced, brace should be newlined:
```cpp
// Note: else if / else should be on same level with '}'
if ( condition_1 )
{
    // Your code
} else if ( condition_2 )
{
    // Your code
} else
{
    // Your code
}

while ( condition )
{
    // Your code
}

for ( int i = 0; i < max_i; i++ )
{
    // Your code
}

switch( value )
{
    case CASE_1:
    // Your code
    
    // Note: if you use fallthrough specify it explicitly with comment
    // Otherwise always use break
    case CASE_2:
    // Your code
    break;
    // Always use default case
    default:
    // Your code
}
```

### Naming rules
* Namespaces should be named using lower case letters.
* Classes should be named using camel case starting with an *upper case* letter.
* Functions and function members should be named using camel case starting with a *lower case* letter.
* Macros should use only *upper case* letters.
* Constant and enums should be named using snake case starting with *k_* prefix.
* Class members names should use *m_* prefix.
* Pointer to variable should use *p_* prefix.

```cpp
// lower case letters for namespace
namespace mincraft
{
// camel case starting with an upper case letter
class SomeUsefulClass
{
public:
    void someFunctionMember();

private:
    // use m_ prefix for class members
    int m_some_member;
};

#define SOME_MACRO( X ) X

// camel case starting with a lower case letter
void 
someFunctionDefinition();

// use k_ for constants/enums
constexpr int k_some_global_const = 0;

enum EnumType
{
    k_enum_constant_1 = 0,
    k_enum_constant_2 = 1
};

// use p_ for pointers
int* p_some_int = nullptr;

}
```
