# C/C++ code style
## Table of content
1. [Requirements](#Requirements)
2. [List of rules](#List_of_rules)
3. [File naming convention ](#file_naming_convention)
4. [Header include](#Header_include)
5. [Header file structure](#Header_file_structure)
6. [Source file structure](#Source_file_structure)
7. [Function declaration](#Function_declaration)
8. [Function definition](#Function_definition)
9. [Control statements](#Control_statements)
10. [Naming rules](#naming_rules)

## Requirements
Anything in the project should be written in one style that stated in this document.
n describing the style, the rules are defined, which must be followed at all times, and the recommendations from which deviations must be under special control and in a minimum number.

## List of rules
### file naming convention
All the file names should use snake case, for example *some_file.cc*
Agreements for using suffixes in filenames:
* .c - C source file,
* .cc - C++ source file,
* .h - C/C++ header file.
### Header include
Every header file should be protected from double include:

```cpp

#ifndef FILE_NAME_H
#define FILE_NAME_H
/**
 * file content.
 */
#endif /* FILE_NAME_H */
 
```
### Header file structure
* Prologue, which contains a comment to the whole file with a description of the functional area of the component.
* Include of non-standard headers and **after** it include of standard headers (stdio.h, etc...) .
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
extern void funcName( int first_parameter,
                      long second_parameter,
                      char* p_third_parameter );
```
### Function definition
Function definition should look like this and placed in source file:
```cpp
/*
Brief function description
*/

static void
funcName( int first_parameter, /* comment for first parameter */
          long second_parameter, /* comment for second parameter */
          char* p_third_parameter ) /* comment for third parameter */
{
printf( "%d %ld %s\\n",
        first_parameter, second_parameter, third_parameter_p );
return;
}
```

### Control statements
Control statement parentheses should be spaced, brace should be newlined:
```cpp
// Note: else if / else should be on same level with '}'
if ( condition_1 )
{
} else if ( condition_2 )
{
} else
{
}

while ( condition )
{
}

for ( int i = 0; i < max_i; i++ )
{
}

```

### Naming rules
* *Classes* should be named using camel case starting with an *upper case* letter.
* Functions and function members should be named using camel case starting with a *lower case* letter.
* Macros should use only *upper case* letters.
* Constant and enums should be named using snake case starting with *k_* prefix.
* Class members names should use *m_* prefix.
* Pointer to variable should use *p_* prefix.
```cpp
// camel case starting with an upper case letter
class SomeUsefulClass
{
public:
    void someFunctionMember();
private:
    // use m_ prefix for class members
    m_some_member;
};

#define SOME_MACRO( X ) X

// camel case starting with a lower case leetter
void someFunctionDefinition();

// use k_ for constants/enums
constexpr int k_some_global_const = 0;

enum EnumType
{
    k_enum_constant_1 = 0,
    k_enum_constant_2 = 1
};

// use p_ for pointers
void someFunction( int* p_some_int );
```
