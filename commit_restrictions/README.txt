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
