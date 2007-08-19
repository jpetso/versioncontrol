$Id$

Version Control API -
An interface to version control systems whose functionality
is provided by pluggable back-end modules.


SHORT DESCRIPTION
-----------------
This is a pure API module, providing functions for interfacing with
version control systems (VCS). In order to work, Version Control API
needs at least one VCS backend module that provides the specific VCS's
functionality.

For the API documentation, have a look at the module file or run doxygen/phpdoc
on it to get a fancier version of the docs.

In subdirectories, you can find three modules that extend the basic
administration functionality of Version Control API with additional
functionality:
- Commit Log displays a history of commits.
- Commit Restrictions grants or denies repository access
  based on path, branch or tag.
- Version Control Account Status requires users to submit motivation texts
  and meet approval of version control administrators before their VCS account
  is enabled.


AUTHOR
------
Jakob Petsovits <jpetso at gmx DOT at>


CREDITS
-------
Some code in Version Control API was taken from the CVS integration module
on drupal.org, its authors deserve a lot of credits and may also hold copyright
for parts of this module.

This module was originally created as part of Google Summer of Code 2007,
so Google deserves some credits for making this possible. Thanks also
to Derek Wright (dww) and Andy Kirkham (AjK) for mentoring
the Summer of Code project.
