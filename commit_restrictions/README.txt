$Id$

Commit Restrictions -
Restrict commits, branches and tags based on item path or branch/tag name.


SHORT DESCRIPTION
-----------------
This module provides the capability to restrict commits based on branch name
or path of the committed items, and to restrict tag assignments based on
the tag name. Restrictions are per-repository, and you can find the settings
for this module as additional item in each add/edit repository form,
in case the VCS backend supports such restrictions.

For actually getting commit restrictions to work, you need to hook the
appropriate (pre-commit) scripts of the respective VCS backend into the
repository's pre-commit hook, so that it can deny commits and/or tags
on the fly.

Commit Restrictions depends on the Version Control API module.


AUTHOR
------
Jakob Petsovits <jpetso at gmx DOT at>


CREDITS
-------
Example regexps and error messages in this module were taken from the
CVS integration module on drupal.org, its authors may hold copyright for
certain parts of this module.

This module was originally created as part of Google Summer of Code 2007,
so Google deserves some credits for making this possible. Thanks also
to Derek Wright (dww) and Andy Kirkham (AjK) for mentoring
the Summer of Code project.
