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
 * Copyright 2007 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */


/**
 * Act on database changes when commits are inserted or deleted.
 * Note that this hook is not necessarily called at the time when the commit
 * actually happens - commits can also be inserted by a cron script when
 * the actual commit has been accomplished for quite a while already.
 *
 * @param $op
 *   'insert' when the commit has just been recorded and inserted into the
 *   database, or 'delete' if it will be deleted right after this hook
 *   has been called.
 *
 * @param $commit
 *   A commit array containing basic information about a commit.
 *   It consists of the following elements:
 *
 *   - 'vc_op_id': The Drupal-specific operation identifier (a simple integer)
 *        which is unique among all operations (commits, branch ops, tag ops)
 *        in all repositories.
 *   - 'type': The type of the operation, which is
 *        VERSIONCONTROL_OPERATION_COMMIT for commits.
 *   - 'repository': The repository where this commit occurred.
 *        This is a structured array, like a single element of
 *        what is returned by versioncontrol_get_repositories().
 *   - 'date': The time when the revision was committed,
 *        given as Unix timestamp.
 *   - 'uid': The Drupal user id of the committer, or 0 if no Drupal user
 *        could be associated to the committer.
 *   - 'username': The system specific VCS username of the committer.
 *   - 'directory': The deepest-level directory in the repository that is
 *        common to all the changed items, e.g. '/src' if the commit changed
 *        the files '/src/subdir/code.php' and '/src/README.txt'.
 *   - 'message': The commit message.
 *   - 'revision': The VCS specific repository-wide revision identifier,
 *        like '' in CVS, '27491' in Subversion or some SHA-1 key in various
 *        distributed version control systems. If there is no such revision
 *        (which may be the case for version control systems that don't support
 *        atomic commits) then the 'revision' element is an empty string.
 *   - '[xxx]_specific': An array of VCS specific additional commit information.
 *        How this array looks like is defined by the corresponding
 *        backend module (versioncontrol_[xxx]).
 *
 * @param $commit_actions
 *   A structured array containing the exact details of what happened to
 *   each item in this commit. Array keys are the current/new paths, also for
 *   VERSIONCONTROL_ACTION_DELETED actions even if the file actually doesn't
 *   exist anymore. The corresponding array values are again structured arrays
 *   and consist of elements with the following keys:
 *
 *   - 'action': Specifies how the item was modified.
 *        One of the predefined VERSIONCONTROL_ACTION_* values.
 *   - 'modified': Boolean value, specifies if a file was modified in addition
 *        to the other action in the 'action' element of the array.
 *        Only exists for the VERSIONCONTROL_ACTION_MOVED
 *        and VERSIONCONTROL_ACTION_COPIED actions.
 *   - 'current item': The updated state of the modified item.
 *        Exists for all actions except VERSIONCONTROL_ACTION_DELETED.
 *   - 'source items': An array with the previous state(s) of the modified item.
 *        Exists for all actions except VERSIONCONTROL_ACTION_ADDED.
 *
 *   Item values are structured arrays and consist of elements
 *   with the following keys:
 *
 *   - 'type': Specifies the item type, which is either
 *        VERSIONCONTROL_ITEM_FILE or VERSIONCONTROL_ITEM_DIRECTORY.
 *   - 'path': The path of the item at the specific revision.
 *   - 'revision': The (file-level) revision when the item was changed.
 *        If there is no such revision (which may be the case for
 *        directory items) then the 'revision' element is an empty string.
 *   - '[xxx]_specific': May be set by the backend to remember additional
 *        item info. ("[xxx]" is the unique string identifier of the respective
 *        version control system.)
 *
 * @ingroup Commits
 * @ingroup Commit notification
 * @ingroup Database change notification
 */
function hook_versioncontrol_commit($op, $commit, $commit_actions) {
  if ($op == 'insert' && module_exists('commitlog')) {
    $to = variable_get('versioncontrol_email_address', 'versioncontrol@example.com');
    commitlog_send_commit_mail($to, $commit, $commit_actions);
  }
}

/**
 * Act on database changes when branch operations are inserted or deleted.
 * Note that this hook is not necessarily called at the time when the branch
 * operation actually happens - they can also be inserted by a cron script when
 * the actual operation has been accomplished for quite a while already.
 *
 * @param $op
 *   'insert' when the branch operation has just been recorded and inserted
 *   into the database, or 'delete' if it will be deleted right after this hook
 *   has been called.
 *
 * @param $branch
 *   A structured array that consists of the following elements:
 *
 *   - 'vc_op_id': The Drupal-specific operation identifier (a simple integer)
 *        which is unique among all operations (commits, branch ops, tag ops)
 *        in all repositories.
 *   - 'type': The type of the operation, which is
 *        VERSIONCONTROL_OPERATION_BRANCH for branches.
 *   - 'branch_name': The name of the target branch
 *        (a string like 'DRUPAL-6--1').
 *   - 'action': Specifies what happened to the branch. This is
 *        VERSIONCONTROL_ACTION_ADDED if the branch was created,
 *        VERSIONCONTROL_ACTION_MODIFIED if was renamed,
 *        or VERSIONCONTROL_ACTION_DELETED if was deleted.
 *   - 'date': The time when the branching was done, given as Unix timestamp.
 *   - 'uid': The Drupal user id of the committer, or 0 if no Drupal user
 *        could be associated to the committer.
 *   - 'username': The system specific VCS username of the committer.
 *   - 'repository': The repository where the branching occurred,
 *        given as a structured array, like the return value
 *        of versioncontrol_get_repository().
 *   - 'directory': The deepest-level directory in the repository that is
 *        common to all the branched items, e.g. '/src' if the files
 *        '/src/subdir/code.php' and '/src/README.txt' were branched.
 *   - '[xxx]_specific': An array of VCS specific additional branch operation
 *        info. How this array looks like is defined by the corresponding
 *        backend module (versioncontrol_[xxx]).
 *
 * @param $branched_items
 *   An array of all items that were affected by the branching operation.
 *   An empty result array means that the whole repository has been branched.
 *   Item values are structured arrays and consist of elements
 *   with the following keys:
 *
 *   - 'type': Specifies the item type, which is either
 *        VERSIONCONTROL_ITEM_FILE or VERSIONCONTROL_ITEM_DIRECTORY.
 *   - 'path': The path of the item at the specific revision.
 *   - 'revision': The (file-level) revision when the item was changed.
 *        If there is no such revision (which may be the case for
 *        directory items) then the 'revision' element is an empty string.
 *   - 'source branch': Optional, may be set by the backend if the
 *        source branch (the one that this one branched off) can be retrieved.
 *        If given, this is a string with the original branch name.
 *   - '[xxx]_specific': May be set by the backend to remember additional
 *        item info. ("[xxx]" is the unique string identifier of the respective
 *        version control system.)
 *
 * @ingroup Branches
 * @ingroup Commit notification
 * @ingroup Database change notification
 */
function hook_versioncontrol_branch_operation($op, $branch, $branched_items) {
  if ($op == 'insert') {
    if (variable_get('commitlog_send_notification_mails', 0)) {
      $to = variable_get('versioncontrol_email_address', 'versioncontrol@example.com');
      commitlog_send_branch_mail('branch', $to, $branch, $branched_items);
    }
  }
}

/**
 * Act on database changes when tag operations are inserted or deleted.
 * Note that this hook is not necessarily called at the time when the tag
 * operation actually happens - they can also be inserted by a cron script when
 * the actual operation has been accomplished for quite a while already.
 *
 * @param $op
 *   'insert' when the tag operation has just been recorded and inserted
 *   into the database, or 'delete' if it will be deleted right after this hook
 *   has been called.
 *
 * @param $tag
 *   A structured array that consists of the following elements:
 *
 *   - 'vc_op_id': The Drupal-specific operation identifier (a simple integer)
 *        which is unique among all operations (commits, branch ops, tag ops)
 *        in all repositories.
 *   - 'type': The type of the operation, which is
 *        VERSIONCONTROL_OPERATION_TAG for tags.
 *   - 'tag_name': The name of the tag (a string like 'DRUPAL-6--1-1').
 *   - 'action': Specifies what happened to the tag. This is
 *        VERSIONCONTROL_ACTION_ADDED if the tag was created,
 *        VERSIONCONTROL_ACTION_MOVED if was renamed,
 *        or VERSIONCONTROL_ACTION_DELETED if was deleted.
 *   - 'date': The time when the tagging was done, given as Unix timestamp.
 *   - 'uid': The Drupal user id of the committer, or 0 if no Drupal user
 *        could be associated to the committer.
 *   - 'username': The system specific VCS username of the committer.
 *   - 'repository': The repository where the tagging occurred,
 *        given as a structured array, like the return value
 *        of versioncontrol_get_repository().
 *   - 'directory': The deepest-level directory in the repository that is
 *        common to all the tagged items, e.g. '/src' if the files
 *        '/src/subdir/code.php' and '/src/README.txt' were tagged.
 *   - 'message': The tag message that the user has given. If the version
 *        control system doesn't support tag messages, this is an empty string.
 *   - '[xxx]_specific': An array of VCS specific additional tag operation info.
 *        How this array looks like is defined by the corresponding
 *        backend module (versioncontrol_[xxx]).
 *
 * @param $tagged_items
 *   An array of all items that were affected by the tagging operation.
 *   An empty result array means that the whole repository has been tagged.
 *   Item values are structured arrays and consist of elements
 *   with the following keys:
 *
 *   - 'type': Specifies the item type, which is either
 *        VERSIONCONTROL_ITEM_FILE or VERSIONCONTROL_ITEM_DIRECTORY.
 *   - 'path': The path of the item at the specific revision.
 *   - 'revision': The (file-level) revision when the item was changed.
 *        If there is no such revision (which may be the case for
 *        directory items) then the 'revision' element is an empty string.
 *   - 'source branch': Optional, may be set by the backend if the
 *        source branch (the one that this tag comes from) can be retrieved.
 *        If given, this is a string with the original branch name.
 *   - '[xxx]_specific': May be set by the backend to remember additional
 *        item info. ("[xxx]" is the unique string identifier of the respective
 *        version control system.)
 *
 * @ingroup Tags
 * @ingroup Commit notification
 * @ingroup Database change notification
 */
function hook_versioncontrol_tag_operation($op, $tag, $tagged_items) {
  if ($op == 'insert') {
    if (variable_get('commitlog_send_notification_mails', 0)) {
      $to = variable_get('versioncontrol_email_address', 'versioncontrol@example.com');
      commitlog_send_tag_mail('tag', $to, $tag, $tagged_items);
    }
  }
}


/**
 * Restrict, ignore or explicitly allow commits for repositories that are
 * connected to the Version Control API by VCS specific hook scripts.
 *
 * @param $commit
 *   A commit array of the commit that is about to happen. As it's not
 *   committed yet, it's not yet in the database as well, which means that
 *   any commit info retrieval functions won't work on this commit array.
 *   It also means there's no 'vc_op_id', 'revision' and 'date' elements like
 *   in regular commit arrays. The 'message' element might or might not be set.
 *   Summed up, here's what this array contains for sure:
 *
 *   - 'type': The type of the operation, which is
 *        VERSIONCONTROL_OPERATION_COMMIT for commits.
 *   - 'repository': The repository where this commit occurred.
 *        This is a structured array, like a single element of
 *        what is returned by versioncontrol_get_repositories().
 *   - 'uid': The Drupal user id of the committer, or 0 if no Drupal user
 *        could be associated to the committer.
 *   - 'username': The system specific VCS username of the committer.
 *   - 'directory': The deepest-level directory in the repository that is
 *        common to all the changed items, e.g. '/src' if the commit changed
 *        the files '/src/subdir/code.php' and '/src/README.txt'.
 *   - '[xxx]_specific': An array of VCS specific additional commit information.
 *        How this array looks like is defined by the corresponding
 *        backend module (versioncontrol_[xxx]).
 *
 * @param $commit_actions
 *   The commit actions of the above commit that is about to happen.
 *   Further information retrieval functions won't work on this array as well.
 *   Also, the 'source items' element of each action and the 'revision' element
 *   of each item in these actions might not be set.
 * @param $branch
 *   The target branch where the commit will happen (a string like 'DRUPAL-5').
 *   If the respective backend doesn't support branches,
 *   this may be NULL instead.
 *
 * @return
 *   An array with error messages (without trailing newlines) if the commit
 *   should not be allowed, or an empty array if you're indifferent,
 *   or TRUE if the commit should be allowed no matter what other
 *   commit access callbacks say.
 *
 * @ingroup Commits
 * @ingroup Commit access
 * @ingroup Target audience: Commit access modules
 */
function hook_versioncontrol_commit_access($commit, $commit_actions, $branch = NULL) {
  if (empty($commit_actions)) {
    return array(); // no idea if this is ever going to happen, but let's be prepared
  }

  // Only allow users with a registered Drupal user account to commit.
  if ($commit['uid'] != 0) {
    $user = user_load(array('uid' => $commit['uid']));
  }
  if (!$user) {
    $backends = versioncontrol_get_backends();
    $backend = $backends[$commit['repository']['vcs']];

    $error_message = t(
"** ERROR: no Drupal user matches !vcs user '!user'.
** Please contact a !vcs administrator for help.",
      array('!vcs' => $backend['name'], '!user' => $commit['username'])
    );
    return array($error_message); // disallow the commit with an explanation
  }
  return array(); // we're indifferent, allow if nobody else has objections
}

/**
 * Restrict, ignore or explicitly allow if branches may be created, renamed
 * or deleted in repositories that are connected to the Version Control API
 * by VCS specific hook scripts.
 *
 * @param $branch
 *   A structured array that consists of the following elements:
 *
 *   - 'type': The type of the operation, which is
 *        VERSIONCONTROL_OPERATION_BRANCH for branches.
 *   - 'branch_name': The name of the target branch
 *        (a string like 'DRUPAL-6--1').
 *   - 'action': Specifies what is going to happen with the branch. This is
 *        VERSIONCONTROL_ACTION_ADDED if the branch is being created,
 *        VERSIONCONTROL_ACTION_MOVED if it's being renamed,
 *        or VERSIONCONTROL_ACTION_DELETED if it is slated for deletion.
 *   - 'uid': The Drupal user id of the committer, or 0 if no Drupal user
 *        could be associated to the committer.
 *   - 'username': The system specific VCS username of the committer.
 *   - 'repository': The repository where the branching occurs,
 *        given as a structured array, like the return value
 *        of versioncontrol_get_repository().
 *   - 'directory': The deepest-level directory in the repository that is
 *        common to all of the branched items.
 *
 * @param $branched_items
 *   An array of all items that are affected by the branching operation.
 *   Compared to standard item arrays, the ones in here may not have the
 *   'revision' element set and can optionally contain a 'source branch'
 *   element that specifies the original branch name of this item.
 *   (For $op == 'delete', 'source branch' is never set.)
 *   An empty $branched_items array means that the whole repository has been
 *   branched.
 *
 * @return
 *   An array with error messages (without trailing newlines) if the branch
 *   may not be assigned, or an empty array if you're indifferent,
 *   or TRUE if the branch may be assigned no matter what other
 *   branch access callbacks say.
 *
 * @ingroup Branches
 * @ingroup Commit access
 * @ingroup Target audience: Commit access modules
 */
function hook_versioncontrol_branch_access($branch, $branched_items) {
  if ($branch['action'] == VERSIONCONTROL_ACTION_DELETED) {
    return array(); // even invalid tags should be allowed to be deleted
  }

  // Make sure that the assigned branch name is allowed.
  $valid_branches = array('@^HEAD$@', '@^DRUPAL-5(--[2-9])?$@', '@^DRUPAL-6--[1-9]$@');

  foreach ($valid_branches as $valid_branch_regexp) {
    if (preg_match($valid_branch_regexp, $branch['branch_name'])) {
      return array(); // we're indifferent, allow if nobody else has objections
    }
  }
  // No branch regexps match this branch, so deny it.
  $error_message = t(
    '** ERROR: the !name branch is not allowed in this repository.',
    array('!name' => $branch['branch_name'])
  );
  return array($error_message); // disallow the commit with an explanation
}

/**
 * Restrict, ignore or explicitly allow if branches may be created, renamed
 * or deleted in repositories that are connected to the Version Control API
 * by VCS specific hook scripts.
 *
 * @param $tag
 *   A structured array that consists of the following elements:
 *
 *   - 'type': The type of the operation, which is
 *        VERSIONCONTROL_OPERATION_TAG for tags.
 *   - 'tag_name': The name of the tag (a string like 'DRUPAL-6--1-1').
 *   - 'action': Specifies what is going to happen with the tag. This is
 *        VERSIONCONTROL_ACTION_ADDED if the tag is being created,
 *        VERSIONCONTROL_ACTION_MOVED if it's being renamed,
 *        or VERSIONCONTROL_ACTION_DELETED if it is slated for deletion.
 *   - 'uid': The Drupal user id of the committer, or 0 if no Drupal user
 *        could be associated to the committer.
 *   - 'username': The system specific VCS username of the committer.
 *   - 'repository': The repository where the tagging occurs,
 *        given as a structured array, like the return value
 *        of versioncontrol_get_repository().
 *   - 'directory': The deepest-level directory in the repository that is
 *        common to all of the tagged items.
 *   - 'message': The tag message that the user has given. If the version
 *        control system doesn't support tag messages, this is an empty string.
 *
 * @param $tagged_items
 *   An array of all items that are affected by the tagging operation.
 *   Compared to standard item arrays, the ones in here may not have the
 *   'revision' element set and can optionally contain a 'source branch'
 *   element that specifies the original branch name of this item.
 *   (For $op == 'move' or $op == 'delete', 'source branch' is never set.)
 *   An empty $tagged_items array means that the whole repository has been
 *   tagged.
 *
 * @return
 *   An array with error messages (without trailing newlines) if the tag
 *   may not be assigned, or an empty array if you're indifferent,
 *   or TRUE if the tag may be assigned no matter what other
 *   tag access callbacks say.
 *
 * @ingroup Tags
 * @ingroup Commit access
 * @ingroup Target audience: Commit access modules
 */
function hook_versioncontrol_tag_access($tag, $tagged_items) {
  if ($tag['action'] == VERSIONCONTROL_ACTION_DELETED) {
    return array(); // even invalid tags should be allowed to be deleted
  }

  // Make sure that the assigned tag name is allowed.
  $valid_tags = array('@^DRUPAL-[56]--(\d+)-(\d+)(-[A-Z0-9]+)?$@');

  foreach ($valid_tags as $valid_tag_regexp) {
    if (preg_match($valid_tag_regexp, $tag['tag_name'])) {
      return array(); // we're indifferent, allow if nobody else has objections
    }
  }
  // No tag regexps match this tag, so deny it.
  $error_message = t(
    '** ERROR: the !name tag is not allowed in this repository.',
    array('!name' => $tag['tag_name'])
  );
  return array($error_message); // disallow the commit with an explanation
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
