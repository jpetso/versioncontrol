$Id$

Version Control Account Status -
Require users to submit a motivation text and meet approval of
version control administrators before their VCS account is enabled.


SHORT DESCRIPTION
-----------------
This module changes the VCS account form so that users need to submit
a motivation text instead of directly registering their accounts.
Until the account is dealt with by a version control administrator,
accounts receive the "queued" status and can then be assigned any of the
"pending", "approved", "declined" or "disabled" statuses.
For all other statuses than "approved", repository access will be denied.

Version Control Account Status depends on the Version Control API module.


AUTHOR
------
Jakob Petsovits <jpetso at gmx DOT at>


CREDITS
-------
Some code in Version Control Account Status was taken from the CVS integration
module on drupal.org, its authors deserve a lot of credits and may also hold
copyright for parts of this module.

This module was originally created as part of Google Summer of Code 2007,
so Google deserves some credits for making this possible. Thanks also
to Derek Wright (dww) and Andy Kirkham (AjK) for mentoring
the Summer of Code project.
