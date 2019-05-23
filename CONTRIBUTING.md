<!-- SPDX-License-Identifier: BSD-3-Clause
     Copyright (c) 2019, Intel Corporation -->

# Contributing to the Multipath TCP Daemon

[Code of Conduct](#code-of-conduct)

[Contributing Changes](#contributing-changes)
  * [Bug Reports](#bug-reports)
  * [Feature Requests](#feature-requests)
  * [Submitting Changes](#submitting-changes)
    * [Testing](#testing)
    * [Code Style](#code-style)
    * [Patch Submission](#patch-submission)

## Code of Conduct
This project is released with a [Contributor Code of
Conduct](CODE_OF_CONDUCT.md). By participating in this project you
agree to abide by its terms.

## Contributing Changes

### Bug Reports
Multipath TCP Daemon bugs are categorized in two high level cases,
*security* and *general*, where each is reported in a different
way described below.

#### Security Bugs
Please see the Multipath TCP Daemon [Security Policy](SECURITY.md) for
details on how to report security related bugs.

#### General Bugs
Please report all general bugs unrelated to security through a GitHub
[issue](https://github.com/intel/mptcpd/issues), using the *Bug
report* template.

### Feature Requests
As with bug report submission, feature requests may be created through
a GitHub [issue](https://github.com/intel/mptcpd/issues), using the
provided *Feature request* template.

### Submitting Changes
Please ensure contributions do not introduce regressions, and conform
to `mptcpd` programming conventions prior to submission, as described below.

#### Testing
All changes should be tested prior to submission.  The `mptcpd` source
distribution contains several ways to run tests:

##### Unit Tests
The `mptcpd` unit test suite may be run like so:
```sh
make check
```
or if `Makefile.am` files were modified:
```sh
make distcheck
```
All mptcpd unit tests should pass or fail as expected, with `PASS` or
`XFAIL` results, respectively.  A successful run of the unit test
suite should look similar to the following:
```
PASS: test-plugin
PASS: test-network-monitor
PASS: test-path-manager
PASS: test-commands
PASS: test-configuration
PASS: test-cxx-build
PASS: test-start-stop
XFAIL: test-bad-log-empty
XFAIL: test-bad-log-long
XFAIL: test-bad-log-short
XFAIL: test-bad-option
XFAIL: test-bad-path-manager
XFAIL: test-bad-plugin-dir
============================================================================
Testsuite summary for mptcpd 0.1a
============================================================================
# TOTAL: 13
# PASS:  7
# SKIP:  0
# XFAIL: 6
# FAIL:  0
# XPASS: 0
# ERROR: 0
============================================================================
```
Source distribution tar archive creation should also succeed when
running `make distcheck`.

##### Code Coverage
Newly added code should be exercised and verified through a unit test
whenever possible.  See the [README](README.md#code-coverage) file for
additional information about enabling code coverage instrumentation,
and results.

##### Installation Tests
In addition to unit tests, the `mptcpd` source distribution provides a
way to run checks after installation:
```sh
sudo make install
make installcheck
sudo make uninstall
```
<br>It may be necessary to set the `LD_LIBRARY_PATH` environment
variable prior to running `make installcheck` if the `libmptcpd`
library was installed to a directory that isn't in the dynamic linker
search path, e.g.:
```sh
LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH make installcheck
```

#### Code Style
The Multipath TCP Daemon follows a specific coding style documented
below through various examples.

[ClangFormat](https://clang.llvm.org/docs/ClangFormat.html) may also
be used to automatically format code to a close approximation of the
`mptcpd` code style by running the `clang-format` command with the
style file [`.clang-format`](.clang-format) in the top-level source
directory, e.g.:
```sh
clang-format -i -style=file file_to_be_formatted.c
```
Additional formatting changes may be necessary after running
`clang-format` to bring the code in compliance with the `mptcpd` code
style documented below.

##### Naming Convention
Except for preprocessor macros and symbols, all symbols should use
[`snake_case`](https://en.wikipedia.org/wiki/Snake_case).  Preprocessor
macros and symbols should use `MACRO_CASE`:
```c
#define DOPPLER_EFFECT "Doppler Effect"

static bool is_doppler_effect(char const *str);
{
        return strcmp(str, DOPPLER_EFFECT) == 0;
}
```

##### Indentation
Use an eight space indentation per code block level _without_ tabs, e.g.:
```c
#define MPTCP_GET_NL_ATTR(data, len, attr)                  \
        do {                                                \
                if (validate_attr_len(len, sizeof(*attr)))  \
                        attr = data;                        \
        } while(0)
```
```c
void foo(void)
{
        // 8 spaces per indentation level, no tabs.
        for (int i = 0; i < 20; ++i) {
                printf("%d: ", i);

                if (i % 2 == 0)
                        printf("even\n");
                else
                        printf("odd\n");
        }
}
```

##### Line length
Whenever possible, lines should not be wider than 74 columns:
```c
void fnord(struct mptcpd_nm const *network_monitor,
           mptcpd_nm_callback callback,
           void *data)
{
        // Line length no longer than 74 columns whenever possible.
        for (int i = 0; i < 20; ++i) {
                if (i % 2 == 0) {
                        // String literal split to fit within 74 columns.
                        printf("%d: The quick brown fox jumps over "
                               "the lazy dog\n",
                               i);
                } else {
                        /*
                          Place all function arguments on their own
                          line if line wrap is necessary for them.
                        */
                        mptcpd_nm_foreach_interface(network_monitor,
                                                    callback,
                                                    data);
                }
        }
}
```
##### Parameter Alignment
Function parameters should be aligned on column immediately after the
opening paranethesis, or split one per line if they cannot all fit on
a 74 column line.  If after splitting across multiple lines any of the
parameters extends beyone column 74 shift all parameters down one line
to the next indentation level:
```c
bool hyperbolic_parabaloid_equal(struct hyperbolic_paraboloid const *lhs,
                                 struct hyperbolic_paraboloid const *rhs);

struct hyperbolic_paraboloid const *get_hyperbolic_parabaloid(
       struct topology const *t,
       int *error);
```
The same alignment applies to arguments in function calls, control
statements, etc.

##### Brace Placement
##### Variable Declaration and Initialization
All C99 features are
* Declare only one variable declaration per line.
* Declare variables as close to their first use as possible
  * Mixed Declarations
  * Proximity
  * Scope
* Loops
```c
// Correct: Prefer for-loop initial declaration.
for (int i = 0; i < 100; ++i)
        printf("%d\n", i);
```
```c
// Incorrect
int i;
for (i = 0; i < 100; ++i)
        printf("%d\n", i);
```
* Pointers
* Structures
<br>The asterisk  `*`
##### CV-qualifier Placement
The CV (`const` or `volatile`) type qualifer should be placed to the
right of the type it is qualifying:
```c
// Correct
int  const foo;
char const *bar;
void const *const *const baz;
void fnord(long const *a);
```
```c
// Incorrect
const int foo;
const char *bar;
const void *const *const baz;
void fnord(const long *a);
```
##### Avoid Hardcoded Values
Symbolic constants ...
##### `const` Correctness
##### Parenthesis Space
##### Comments
```c
// One line comment.
```
```c
/*
    Multi-line comment.
    No asterisks leading lines between comment open and close.
    Text begins in the column after the opening '/*'.
*/
```
_Note_: Code documentation comments follow a different convention,
i.e. the [Doxygen](http://www.doxygen.nl/) format.  See [Code
Documentation](#code-documentation) below.
##### Unused arguments
```c
void foo(int i)
{
        // Cast to void to mark as unused.
        (void) i;

        printf("Hello world!\n");
}
```
##### Unnecessary `if/else` Blocks
```c
// Correct
bool is_valid(int n)
{
        static int const MAX_N = 100;

        return n > 0 && n <= MAX_N;
}
```
```c
// Incorrect
bool is_valid(int n)
{
        static int const MAX_N = 100;

        if (n > 0)
                if (n <= MAX_N)
                        return true;

        return false;
}
```
##### Exported Symbols
##### Code Documentation
Document all code using [Doxygen](http://www.doxygen.nl/)
documentation comments, e.g.:
```c
/**
 * @struct foo
 *
 * @brief Short description.
 *
 * Long description.
 */
struct foo
{
        /// Single line documentation of field.
        int bar;

        /**
         * Multi-line
         * documentation.
         */
        int baz;
};
```
Refer to the Doxygen [Special
Commands](http://www.doxygen.nl/manual/commands.html) documentation
for details on commands like `@struct` used above.
#### Patch Submission
Please submit patches through a [pull
request](https://help.github.com/en/articles/about-pull-requests).
The GitHub documentation that describes how to [fork a
repository](https://help.github.com/en/articles/fork-a-repo) provides
a good explanation of how to submit changes.  Ideally, the pull
request should be associated with a bug report or feature request.
Remember to [reference the
issue](https://help.github.com/en/articles/closing-issues-using-keywords)
number in the pull request commit message.
##### Git Commit Message
