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
A good amount of code in Commit Log was taken from the CVS integration
module on drupal.org, where the adapted sections were committed by:

Derek Wright ("dww", http://drupal.org/user/46549)
Karthik ("Zen", http://drupal.org/user/21209)

This module was originally created as part of Google Summer of Code 2007,
so Google also deserves some credits for making this possible.
