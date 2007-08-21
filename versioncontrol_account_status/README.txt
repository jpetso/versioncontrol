$Id$

Version Control Account Status -
Require users to submit a motivation text and meet approval of
version control administrators before their VCS account is enabled.


SHORT DESCRIPTION
-----------------
This module changes the VCS account form so that users need to submit
a motivation text instead of directly registering their accounts.
Until the account is dealt with by a version control administrator,
accounts receive the "Queued" status. The administrator can assign any
of the following account status values:

VERSIONCONTROL_ACCOUNT_STATUS_QUEUED:
  The user has applied for an approval, but the VCS administrator
  hasn't yet looked at the application.
VERSIONCONTROL_ACCOUNT_STATUS_PENDING:
  The administrator needs more information from the applicant
  in order to properly evaluate the application.
VERSIONCONTROL_ACCOUNT_STATUS_APPROVED:
  The application has been evaluated and approved,
  and the user may have access to the repository.
VERSIONCONTROL_ACCOUNT_STATUS_DECLINED:
  The application has been evaluated and disapproved.
  The user can, however, reapply.
VERSIONCONTROL_ACCOUNT_STATUS_DISABLED:
  The application had been approved in the past, but was revoked.
  The user doesn't have repository access anymore.

For all other statuses than "Approved", repository access will be denied.

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
