<?php
// $Id$
/**
 * @file
 * Version Control API - An interface to version control systems
 * whose functionality is provided by pluggable back-end modules.
 *
 * This file contains module hooks for users of Version Control API,
 * with API documentation and a bit of example code.
 * Hooks that are intended for VCS backends are not to be found in this file
 * as they are already documented in versioncontrol_fakevcs.module.
 *
 * Copyright 2007, 2008 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */


/**
 * Act on database changes when commit, tag or branch operations are inserted
 * or deleted. Note that this hook is not necessarily called at the time
 * when the operation actually happens - operations can also be inserted
 * by a cron script when the actual commit/branch/tag has been accomplished
 * for quite a while already.
 *
 * @param $op
 *   'insert' when the operation has just been recorded and inserted into the
 *   database, or 'delete' if it will be deleted right after this hook
 *   has been called.
 *
 * @param $operation
 *   An operation array containing basic information about the commit, branch
 *   or tag operation. It consists of the following elements:
 *
 *   - 'vc_op_id': The Drupal-specific operation identifier (a simple integer)
 *        which is unique among all operations (commits, branch ops, tag ops)
 *        in all repositories.
 *   - 'type': The type of the operation - one of the
 *        VERSIONCONTROL_OPERATION_{COMMIT,BRANCH,TAG} constants.
 *        Note that if you pass branch or tag constraints, this function might
 *        nevertheless return commit operations too - that happens for version
 *        control systems without native branches or tags (like Subversion)
 *        when a branch or tag is affected by the commit.
 *   - 'repository': The repository where this operation occurred.
 *        This is a structured "repository array", like is returned
 *        by versioncontrol_get_repository().
 *   - 'date': The time when the operation was performed, given as
 *        Unix timestamp. (For commits, this is the time when the revision
 *        was committed, whereas for branch/tag operations it is the time
 *        when the files were branched or tagged.)
 *   - 'uid': The Drupal user id of the operation author, or 0 if no
 *        Drupal user could be associated to the author.
 *   - 'username': The system specific VCS username of the author.
 *   - 'message': The log message for the commit, tag or branch operation.
 *        If a version control system doesn't support messages for any of them,
 *        this element contains an empty string.
 *   - 'revision': The VCS specific repository-wide revision identifier,
 *        like '' in CVS, '27491' in Subversion or some SHA-1 key in various
 *        distributed version control systems. If there is no such revision
 *        (which may be the case for version control systems that don't support
 *        atomic commits) then the 'revision' element is an empty string.
 *        For branch and tag operations, this element indicates the
 *        (repository-wide) revision of the files that were branched or tagged.
 *
 *   - 'labels': An array of branches or tags that were affected by this
 *        operation. Branch and tag operations are known to only affect one
 *        branch or tag, so for these there will be only one element (with 0
 *        as key) in 'labels'. Commits might affect any number of branches,
 *        including none. Commits that emulate branches and/or tags (like
 *        in Subversion, where they're not a native concept) can also include
 *        add/delete/move operations for labels, as detailed below.
 *        Mind that the main development branch - e.g. 'HEAD', 'trunk'
 *        or 'master' - is also considered a branch. Each element in 'labels'
 *        is a structured array with the following keys:
 *
 *        - 'id': The label identifier (a simple integer), used for unique
 *             identification of branches and tags in the database.
 *        - 'name': The branch or tag name (a string).
 *        - 'action': Specifies what happened to this label in this operation.
 *             For plain commits, this is always VERSIONCONTROL_ACTION_MODIFIED.
 *             For branch or tag operations (or commits that emulate those),
 *             it can be either VERSIONCONTROL_ACTION_ADDED or
 *             VERSIONCONTROL_ACTION_DELETED.
 *
 * @param $operation_items
 *   A structured array containing all items that were affected by the above
 *   operation. Array keys are the current/new paths, even if the item doesn't
 *   exist anymore (as is the case with delete actions in commits).
 *   The associated array elements are structured item arrays and consist of
 *   the following elements:
 *
 *   - 'type': Specifies the item type, which is either
 *        VERSIONCONTROL_ITEM_FILE or VERSIONCONTROL_ITEM_DIRECTORY for items
 *        that still exist, or VERSIONCONTROL_ITEM_FILE_DELETED respectively
 *        VERSIONCONTROL_ITEM_DIRECTORY_DELETED for items that have been
 *        removed (by a commit's delete action).
 *   - 'path': The path of the item at the specific revision.
 *   - 'revision': The (file-level) revision when the item was changed.
 *        If there is no such revision (which may be the case for
 *        directory items) then the 'revision' element is an empty string.
 *   - 'item_revision_id': Identifier of this item revision in the database.
 *        Note that you can only rely on this element to exist for
 *        operation items - functions that interface directly with the VCS
 *        (such as versioncontrol_get_directory_contents() or
 *        versioncontrol_get_parallel_items()) might not include
 *        this identifier, for obvious reasons.
 *
 *   For commit operations, additional information about the origin of
 *   the items is also available. The following elements will be set
 *   for each item in addition to the ones listed above:
 *
 *   - 'action': Specifies how the item was changed.
 *        One of the predefined VERSIONCONTROL_ACTION_* values.
 *   - 'source_items': An array with the previous state(s) of the affected item.
 *        Empty if 'action' is VERSIONCONTROL_ACTION_ADDED.
 *   - 'replaced_item': The previous but technically unrelated item at the
 *        same location as the current item. Only exists if this previous item
 *        was deleted and replaced by a different one that was just moved
 *        or copied to this location.
 *
 * @ingroup Operations
 * @ingroup Commit notification
 * @ingroup Database change notification
 */
function hook_versioncontrol_operation($op, $operation, $operation_items) {
  if ($op == 'insert' && module_exists('commitlog')) {
    if (variable_get('commitlog_send_notification_mails', 0)) {
      $mailto = variable_get('versioncontrol_email_address', 'versioncontrol@example.com');
      commitlog_notification_mail($mailto, $operation, $operation_items);
    }
  }
}


/**
 * Restrict, ignore or explicitly allow a commit, branch or tag operation
 * for a repository that is connected to the Version Control API
 * by VCS specific hook scripts.
 *
 * @param $operation
 *   A single operation array like the ones returned by
 *   versioncontrol_get_operations(), but leaving out on a few details that
 *   will instead be determined by this function. This array describes
 *   the operation that is about to happen. As it's not committed yet,
 *   it's also not in the database yet, which means that any information
 *   retrieval functions won't work on this operation array.
 *   It also means there's no 'vc_op_id', 'revision' and 'date' elements like
 *   in regular operation arrays. The 'message' element will not be set
 *   if the VCS doesn't support log messages for the current operation
 *   (e.g., most version control systems don't have branch messages).
 *
 *   Summed up, here's what this array contains for sure:
 *
 *   - 'type': The type of the operation - one of the
 *        VERSIONCONTROL_OPERATION_{COMMIT,BRANCH,TAG} constants.
 *   - 'repository': The repository where this operation occurs,
 *        given as a structured array, like the return value
 *        of versioncontrol_get_repository().
 *   - 'uid': The Drupal user id of the committer.
 *   - 'username': The system specific VCS username of the committer.
 *
 *   - 'labels': An array of branches or tags that will be affected by this
 *        operation. Branch and tag operations are known to only affect one
 *        branch or tag, so for these there will be only one element (with 0
 *        as key) in 'labels'. Commits might affect any number of branches,
 *        including none. Commits that emulate branches and/or tags (like
 *        in Subversion, where they're not a native concept) can also include
 *        add/delete/move operations for labels, as detailed below.
 *        Mind that the main development branch - e.g. 'HEAD', 'trunk'
 *        or 'master' - is also considered a branch. Each element in 'labels'
 *        is a structured array with the following keys:
 *
 *        - 'name': The branch or tag name (a string).
 *        - 'action': Specifies what happened to this label in this operation.
 *             For plain commits, this is always VERSIONCONTROL_ACTION_MODIFIED.
 *             For branch or tag operations (or commits that emulate those),
 *             it can be either VERSIONCONTROL_ACTION_ADDED or
 *             VERSIONCONTROL_ACTION_DELETED.
 *
 * @param $operation_items
 *   A structured array containing the exact details of what is about to happen
 *   to each item in this commit. The structure of this array is the same as
 *   the return value of versioncontrol_get_operation_items() - that is,
 *   elements for 'type', 'path' and 'revision' - but doesn't include
 *   the 'item_revision_id' element as there's no relation to the database yet.
 *
 *   The 'action', 'source_items', 'replaced_item' and 'revision' elements
 *   of each item are optional for the VCS backend and may be left unset.
 *
 * @return
 *   An array with error messages (without trailing newlines) if the operation
 *   should not be allowed, or an empty array if you're indifferent,
 *   or TRUE if the operation should be allowed no matter what other
 *   write access callbacks say.
 *
 * @ingroup Operations
 * @ingroup Commit access
 * @ingroup Target audience: Commit access modules
 */
function hook_versioncontrol_write_access($operation, $operation_items) {
  // Only allow users with a registered Drupal user account to commit.
  if ($operation['uid'] != 0) {
    $user = user_load(array('uid' => $operation['uid']));
  }
  if (!$user) {
    $backend = versioncontrol_get_backend($operation['repository']);

    $error_message = t(
"** ERROR: no Drupal user matches !vcs user '!user'.
** Please contact a !vcs administrator for help.",
      array('!vcs' => $backend['name'], '!user' => $operation['username'])
    );
    return array($error_message); // disallow the commit with an explanation
  }

  // Mind that also commits normally have labels, except for stuff like
  // Subversion when the user commits outside of the trunk/branches/tags
  // directories. Let's say we want to prevent such commits.
  if (empty($operation['labels'])) {
    $error_message = t("** ERROR: It is not allowed to commit without a branch or tag!");
    return array($error_message);
  }

  // If an empty array is returned then that means we're indifferent:
  // allow the operation if nobody else has objections.
  $error_messages = array();

  // Restrict disallowed branches and tags.
  $valid_labels = array(
    VERSIONCONTROL_OPERATION_BRANCH => array('@^HEAD$@', '@^DRUPAL-5(--[2-9])?$@', '@^DRUPAL-6--[1-9]$@'),
    VERSIONCONTROL_OPERATION_TAG => array('@^DRUPAL-[56]--(\d+)-(\d+)(-[A-Z0-9]+)?$@'),
  );

  foreach ($operation['labels'] as $label) {
    if ($label['type'] == VERSIONCONTROL_OPERATION_TAG
        && $label['action'] == VERSIONCONTROL_ACTION_DELETED) {
      continue; // no restrictions, even invalid tags should be allowed to be deleted
    }

    // Make sure that the assigned branch or tag name is allowed.
    $valid = FALSE;

    foreach ($valid_labels[$label['type']] as $valid_label_regexp) {
      if (preg_match($valid_label_regexp, $label['name'])) {
        $valid = TRUE;
        break;
      }
    }
    if (!$valid) {
      // No regexps match this label, so deny it.
      $error_messages[] = t('** ERROR: the !name !labeltype is not allowed in this repository.', array(
        '!name' => $label['name'],
        '!labeltype' => ($label['type'] == VERSIONCONTROL_OPERATION_BRANCH)
                        ? t('branch')
                        : t('tag'),
      ));
    }
  }

  return $error_messages;
}


/**
 * Extract repository data from the repository editing/adding form's
 * submitted values. This data will be passed to
 * hook_versioncontrol_repository() as part of the repository array.
 *
 * @param $form_values
 *   The form values that were submitted in the repository editing/adding form.
 *   If you altered this form ($form['#id'] == 'versioncontrol-repository-form')
 *   and added an additional form element then this parameter will also contain
 *   the value of this form element.
 *
 * @return
 *   An array of elements that will be merged into the repository array.
 *
 * @ingroup Repositories
 * @ingroup Form handling
 * @ingroup Target audience: All modules with repository specific settings
 */
function hook_versioncontrol_extract_repository_data($form_values) {
  // The user can specify multiple repository ponies, separated by whitespace.
  // So, split the string up into an array of ponies.
  $ponies = trim($form_values['mymodule_ponies']);
  $ponies = empty($ponies) ? array() : explode(' ', $ponies);

  return array(
    'mymodule' => array(
      'ponies' => $ponies,
    ),
  );
}

/**
 * Act on database changes when VCS repositories are inserted,
 * updated or deleted.
 *
 * @param $op
 *   Either 'insert' when the repository has just been created, or 'update'
 *   when repository name, root, URL backend or module specific data change,
 *   or 'delete' if it will be deleted after this function has been called.
 *
 * @param $repository
 *   The repository array containing the repository. It's a single
 *   repository array like the one returned by versioncontrol_get_repository(),
 *   so it consists of the following elements:
 *
 *   - 'repo_id': The unique repository id.
 *   - 'name': The user-visible name of the repository.
 *   - 'vcs': The unique string identifier of the version control system
 *        that powers this repository.
 *   - 'root': The root directory of the repository. In most cases,
 *        this will be a local directory (e.g. '/var/repos/drupal'),
 *        but it may also be some specialized string for remote repository
 *        access. How this string may look like depends on the backend.
 *   - 'authorization_method': The string identifier of the repository's
 *        authorization method, that is, how users may register accounts
 *        in this repository. Modules can provide their own methods
 *        by implementing hook_versioncontrol_authorization_methods().
 *   - 'url_backend': The prefix (excluding the trailing underscore)
 *        for URL backend retrieval functions.
 *   - '[xxx]_specific': An array of VCS specific additional repository
 *        information. How this array looks like is defined by the
 *        corresponding backend module (versioncontrol_[xxx]).
 *   - '???': Any other additions that modules added by implementing
 *        versioncontrol_extract_repository_data().
 *
 * @ingroup Repositories
 * @ingroup Database change notification
 * @ingroup Form handling
 * @ingroup Target audience: All modules with repository specific settings
 */
function hook_versioncontrol_repository($op, $repository) {
  $ponies = $repository['mymodule']['ponies'];

  switch ($op) {
    case 'update':
      db_query("DELETE FROM {mymodule_ponies}
                WHERE repo_id = %d", $repository['repo_id']);
      // fall through
    case 'insert':
      foreach ($ponies as $pony) {
        db_query("INSERT INTO {mymodule_ponies} (repo_id, pony)
                  VALUES (%d, %s)", $repository['repo_id'], $pony);
      }
      break;

    case 'delete':
      db_query("DELETE FROM {mymodule_ponies}
                WHERE repo_id = %d", $repository['repo_id']);
      break;
  }
}


/**
 * Register new authorization methods that can be selected for a repository.
 * A module may restrict access and alter forms depending on the selected
 * authorization method which is a property of every repository array
 * ($repository['authorization_method']).
 *
 * A list of all authorization methods can be retrieved
 * by calling versioncontrol_get_authorization_methods().
 *
 * @return
 *   A structured array containing information about authorization methods
 *   provided by this module, wrapped in a structured array. Array keys are
 *   the unique string identifiers of each authorization method, and
 *   array values are the user-visible method descriptions (wrapped in t()).
 *
 * @ingroup Accounts
 * @ingroup Authorization
 * @ingroup Target audience: Authorization control modules
 */
function hook_versioncontrol_authorization_methods() {
  return array(
    'mymodule_code_ninja' => t('Code ninja skills required'),
  );
}

/**
 * Alter the list of repositories that are available for user registration
 * and editing.
 *
 * @param $repository_names
 *   The list of repository names as it is shown in the select box
 *   at 'versioncontrol/register'. Array keys are the repository ids,
 *   and array elements are the captions in the select box.
 *   There's two things that can be done with this array:
 *   - Change (amend) the caption, in order to provide more information
 *     for the user. (E.g. note that an application is necessary.)
 *   - Unset any number of array elements. If you do so, the user will not
 *     be able to register a new account for this repository.
 * @param $repositories
 *   A list of repositories (with the repository ids as array keys) that
 *   includes at least all of the repositories that correspond to the
 *   repository ids of the @p $repository_names array.
 *
 * @ingroup Accounts
 * @ingroup Authorization
 * @ingroup Repositories
 * @ingroup Form handling
 * @ingroup Target audience: Authorization control modules
 */
function hook_versioncontrol_alter_repository_selection(&$repository_names, $repositories) {
  global $user;

  foreach ($repository_names as $repo_id => $caption) {
    if ($repositories[$repo_id]['authorization_method'] == 'mymodule_code_ninja') {
      if (!in_array('code ninja', $user->roles)) {
        unset($repository_names[$repo_id]);
      }
    }
  }
}

/**
 * Let the Version Control API know whether the given VCS account
 * is authorized or not.
 *
 * @return
 *   TRUE if the account is authorized, or FALSE if it's not.
 *
 * @ingroup Accounts
 * @ingroup Authorization
 * @ingroup Target audience: Authorization control modules
 */
function hook_versioncontrol_is_account_authorized($uid, $repository) {
  if ($repository['authorization_method'] != 'mymodule_dojo_status') {
    return TRUE;
  }
  $result = db_query("SELECT status
                      FROM {mymodule_dojo_status}
                      WHERE uid = %d AND repo_id = %d",
                      $uid, $repository['repo_id']);

  while ($account = db_fetch_object($result)) {
    return ($account->status == MYMODULE_SENSEI);
  }
  return FALSE;
}


/**
 * Unset filtered accounts before they are even attempted to be displayed
 * on the account list ("admin/project/versioncontrol-accounts").
 * You'll most probably use this in conjunction with an additional filter
 * form element that is added to the account filter form
 * ($form['#id'] == 'versioncontrol-account-filter-form') with form_alter().
 *
 * @param $accounts
 *   The accounts that would normally be displayed, in the same format as the
 *   return value of versioncontrol_get_accounts(). Entries in this list
 *   may be unset by this filter function.
 *
 * @ingroup Accounts
 * @ingroup Form handling
 * @ingroup Target audience: Authorization control modules
 */
function hook_versioncontrol_filter_accounts(&$accounts) {
  if (empty($accounts)) {
    return;
  }
  // Use a default value if the session variable hasn't yet been set.
  if (!isset($_SESSION['mymodule_filter_username'])) {
    $_SESSION['mymodule_filter_username'] = 'chx';
  }
  $mymodule_filter_username = $_SESSION['mymodule_filter_username'];

  if ($mymodule_filter_username == '') {
    return; // Don't change the list if no filtering should happen.
  }

  foreach ($accounts as $uid => $usernames_by_repository) {
    foreach ($usernames_by_repository as $repo_id => $username) {
      if ($username != $mymodule_filter_username) {
        unset($accounts[$uid][$repo_id]);
        if (empty($accounts[$uid])) {
          unset($accounts[$uid]);
        }
      }
    }
  }
}


/**
 * Extract account data from the account editing/creating form's submitted
 * values. This data will be passed to hook_versioncontrol_account()
 * as part of the $additional_data parameter.
 *
 * @param $form_values
 *   The form values that were submitted in the account editing/creating form.
 *   If you altered this form ($form['#id'] == 'versioncontrol-account-form')
 *   and added an additional form element then this parameter will also contain
 *   the value of this form element.
 *
 * @return
 *   An array of elements that will be merged into the $additional_data array.
 *
 * @ingroup Accounts
 * @ingroup Form handling
 * @ingroup Target audience: Commit access modules
 * @ingroup Target audience: Authorization control modules
 * @ingroup Target audience: All modules with account specific settings
 */
function hook_versioncontrol_extract_account_data($form_values) {
  if (empty($form_values['mymodule_karma'])) {
    return array();
  }
  return array(
    'mymodule' => array(
      'karma' => $form_values['mymodule_karma'],
    ),
  );
}

/**
 * Act on database changes when VCS accounts are inserted, updated or deleted.
 *
 * @param $op
 *   Either 'insert' when the account has just been created, 'update'
 *   when it has been updated, or 'delete' if it will be deleted after
 *   this function has been called.
 * @param $uid
 *   The Drupal user id corresponding to the VCS account.
 * @param $username
 *   The VCS specific username (a string) of the account.
 * @param $repository
 *   The repository where the user has its VCS account.
 * @param $additional_data
 *   An array of additional author information. Modules can fill this array
 *   by implementing hook_versioncontrol_extract_account_data().
 *
 * @ingroup Accounts
 * @ingroup Form handling
 * @ingroup Target audience: Commit access modules
 * @ingroup Target audience: Authorization control modules
 * @ingroup Target audience: All modules with account specific settings
 */
function hook_versioncontrol_account($op, $uid, $username, $repository, $additional_data = array()) {
  switch ($op) {
    case 'insert':
    case 'update':
      // Recap: if form_alter() wasn't applied, our array element is not set.
      $mymodule_data = $additional_data['mymodule'];

      if (!isset($mymodule_data)) {
        // In most modules, form_alter() will always be applied to the
        // account editing/creating form. If $mymodule_data is empty
        // nevertheless then it means that the account has been created
        // programmatically rather than with a form submit.
        // In that case, we better assign a default value:
        if ($op == 'insert') {
          $mymodule_data = array('karma' => 50);
        }
        // Don't change the status for programmatical updates, though.
        if ($op == 'update') {
          break;
        }
      }

      db_query("DELETE FROM {mymodule_karma} WHERE uid = %d", $uid);
      db_query("INSERT INTO {mymodule_karma} (uid, karma) VALUES (%d, %d)",
               $uid, $mymodule_data['karma']);
      break;

    case 'delete':
      db_query("DELETE FROM {mymodule_karma} WHERE uid = %d", $uid);
      break;
  }
}

/**
 * Add additional columns into the list of VCS accounts.
 * By changing the @p $header and @p $rows_by_uid arguments,
 * the account list can be customized accordingly.
 *
 * @param $accounts
 *   The list of accounts that is being displayed in the account table. This is
 *   a structured array like the one returned by versioncontrol_get_accounts().
 * @param $repositories
 *   An array of repositories where the given users have a VCS account.
 *   Array keys are the repository ids, and array values are the
 *   repository arrays like returned from versioncontrol_get_repository().
 * @param $header
 *   A list of columns that will be passed to theme('table').
 * @param $rows_by_uid
 *   An array of existing table rows, with Drupal user ids as array keys.
 *   Each row already includes the generic column values, and for each row
 *   there is an account with the same uid given in the @p $accounts parameter.
 *
 * @ingroup Accounts
 * @ingroup Form handling
 * @ingroup Target audience: Authorization control modules
 * @ingroup Target audience: All modules with account specific settings
 */
function hook_versioncontrol_alter_account_list($accounts, $repositories, &$header, &$rows_by_uid) {
  $header[] = t('Karma');

  foreach ($rows_by_uid as $uid => $row) {
    $rows_by_uid[$uid][] = theme('user_karma', $uid);
  }
}
