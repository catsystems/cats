# The C.A.T.S. Computer

<img src="logo/CATS_Smile.png" width="300" height="300">

*Always land on your ~~feet~~ paws...*

## Pushing to Remote Repository

In order to ensure consistency and easier diff review between commits,
*clang-format* should be used to format all modified C/C++ files. 

The entire codebase is already formatted with *clang-format v12.0.0*.

To download `clang-format` visit this [page](https://releases.llvm.org/download.html).

To automatically format the modified code before committing, you should
copy the pre-commit hook from `./hooks` to `./.git/hooks`.

Unfortunately, it is not easily possible to check whether the code is
formatted correctly on the server side (while you are doing a push) so
_**please**_ do this if you are working with C/C++ files. 
