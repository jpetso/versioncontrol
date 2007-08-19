$Id$

Commit Log -
Display a history of commits, optionally filtered by a set
of constraint arguments.


SHORT DESCRIPTION
-----------------
This module provides a page with commit logs of commits known to the
Version Control API. By default, it displays all commits in all repositories,
but the list of commits can be filtered by passing appropriate URLs.
If the Version Control API knows about URLs of repository viewers and
issue trackers, this module provides links to those in the commit logs.

Commit Log depends on the Version Control API module.


AUTHOR
------
Jakob Petsovits <jpetso at gmx DOT at>


CREDITS
-------
A good share of code in Commit Log was taken from the CVS integration module
on drupal.org, its authors deserve a lot of credits and may also hold copyright
for parts of this module.

This module was originally created as part of Google Summer of Code 2007,
so Google deserves some credits for making this possible. Thanks also
to Derek Wright (dww) and Andy Kirkham (AjK) for mentoring
the Summer of Code project.
