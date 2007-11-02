<?php
// $Id$
/**
 * @file
 * Version Control API - An interface to version control systems
 * whose functionality is provided by pluggable back-end modules.
 *
 * This file contains the backend-only side of the Version Control API.
 * It is public API, but not meant to be used by non-backend modules.
 *
 * Copyright 2007 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */

/**
 * Check if the user who is attempting to execute a commit/tag/branch operation
 * has commit access at all, and set an error if that is not the case.
 *
 * @param $object
 *   The commit, branch operation or tag operation array containing
 *   repository and uid of the account that is to be checked.
 *
 * @return
 *   TRUE if the user may create a commit/branch/tag in this repository,
 *   or FALSE otherwise.
 */
function _versioncontrol_has_account_access($object) {
  if (!variable_get('versioncontrol_allow_unauthorized_access', 0)) {
    if (!versioncontrol_is_account_authorized($object['uid'], $object['repository'])) {
      _versioncontrol_access_errors(array(t(
        '** ERROR: !user does not have commit access to this repository.',
        array('!user' => $object['username'], '!commit-branch-or-tag' => $type)
      )));
      return FALSE;
    }
  }
  return TRUE;
}

/**
 * Set an error if the repository for a commit or branch/tag operation
 * can't be retrieved.
 *
 * @param $type
 *   The translated string for 'commit', 'branch' or 'tag'.
 */
function _versioncontrol_set_repository_error($type) {
  _versioncontrol_access_errors(array(t(
'** ERROR: Version Control API cannot determine a repository
** for the !commit-branch-or-tag information given by the VCS backend.',
    array('!commit-branch-or-tag' => $type)
  )));
}

/**
 * Determine if a commit may be executed or not.
 * Call this inside a pre-commit hook.
 *
 * @param $commit
 *   A single commit array like the ones returned by
 *   versioncontrol_get_commits(), but leaving out on a few details that
 *   will instead be determined by this function. This array describes
 *   the commit that is about to happen. Here's the allowed elements:
 *
 *   - 'repository': The repository where this commit occurs, given as a
 *        structured array, like a single element of what is returned
 *        by versioncontrol_get_repositories().
 *        You can either pass this or 'repo_id'.
 *   - 'repo_id': The repository where this commit occurs, given as a simple
 *        integer id. You can either pass this or 'repository'.
 *   - 'uid': The Drupal user id of the committer. Passing this is optional -
 *        if it isn't set, this function will determine the uid.
 *   - 'username': The system specific VCS username of the committer.
 *   - 'message': The commit message. Passing this is optional, just leave the
 *       'message' element unset if the VCS can't retrieve the commit message
 *       at the time of the pre-commit hook.
 *   '[xxx]_specific': An array of VCS specific additional commit information.
 *       How this array looks like is defined by the corresponding
 *       backend module (versioncontrol_[xxx]).
 *
 * @param $commit_actions
 *   A structured array containing the exact details of what is about to happen
 *   to each item in this commit. The structure of this array is the same as
 *   the return value of versioncontrol_get_commit_actions().
 *   The 'source items' element of each action and the 'revision' element of
 *   each item in these actions are optional and may be left unset.
 * @param $branch
 *   The target branch where the commit will happen (a string like 'DRUPAL-5').
 *   If the VCS can't retrieve the branch when calling this function
 *   or doesn't support branches at all, this may be NULL instead.
 *
 * @return
 *   TRUE if the commit may happen, or FALSE if not.
 *   If FALSE is returned, you can retrieve the concerning error messages
 *   by calling versioncontrol_get_access_errors().
 */
function versioncontrol_has_commit_access($commit, $commit_actions, $branch = NULL) {
  $commit = _versioncontrol_fill_commit($commit, $commit_actions);
  if (!isset($commit['repository'])) {
    _versioncontrol_set_repository_error(t('commit'));
    return FALSE;
  }
  if (!_versioncontrol_has_account_access($commit, t('commit'))) {
    return FALSE;
  }

  $error_messages = array();

  foreach (module_implements('versioncontrol_commit_access') as $module) {
    $function = $module .'_versioncontrol_commit_access';

    // If at least one hook_versioncontrol_commit_access returns TRUE,
    // the commit goes through. (This is for admin or sandbox exceptions.)
    $outcome = $function($commit, $commit_actions, $branch);
    if ($outcome === TRUE) {
      return TRUE;
    }
    else { // If !TRUE, $outcome is required to be an array with error messages
      $error_messages = array_merge($error_messages, $outcome);
    }
  }

  // Let the commit fail if there's more than zero error messages.
  if (!empty($error_messages)) {
    _versioncontrol_access_errors($error_messages);
    return FALSE;
  }
  return TRUE;
}

/**
 * Determine if a branch may be assigned to a set of items.
 * Call this inside a pre-commit/pre-branch hook.
 *
 * @param $branch
 *   A structured array that consists of the following elements:
 *
 *   - 'branch_name': The name of the target branch
 *        (a string like 'DRUPAL-6--1').
 *   - 'action': Specifies what is going to happen with the branch. This is
 *        VERSIONCONTROL_ACTION_ADDED if the branch is being created,
 *        VERSIONCONTROL_ACTION_MODIFIED if it's being renamed,
 *        or VERSIONCONTROL_ACTION_DELETED if it is slated for deletion.
 *   - 'uid': The Drupal user id of the committer. Passing this is optional -
 *        if it isn't set, this function will determine the uid.
 *   - 'username': The system specific VCS username of the committer.
 *   - 'repository': The repository where the branching occurs, given as a
 *        structured array, like a single element of what is returned
 *        by versioncontrol_get_repositories().
 *        You can either pass this or 'repo_id'.
 *   - 'repo_id': The repository where the branching occurs, given as a simple
 *        integer id. You can either pass this or 'repository'.
 *   - '[xxx]_specific': An array of VCS specific additional branch operation
 *        info. How this array looks like is defined by the
 *        corresponding backend module (versioncontrol_[xxx]).
 *
 * @param $branched_items
 *   An array of all items that are affected by the branching operation.
 *   Compared to standard item arrays, the ones in here may not have the
 *   'revision' element set and can optionally contain a 'source branch'
 *   element that specifies the original branch of this item.
 *   (For $op == 'delete', 'source branch' is never set.)
 *   An empty $branched_items array means that the whole repository has been
 *   branched.
 *
 * @return
 *   TRUE if the branch may be assigned, or FALSE if not.
 *   If FALSE is returned, you can retrieve the concerning error messages
 *   by calling versioncontrol_get_access_errors().
 */
function versioncontrol_has_branch_access($branch, $branched_items) {
  $branch = _versioncontrol_fill_branch_or_tag($branch, $branched_items);
  if (!isset($branch['repository'])) {
    _versioncontrol_set_repository_error(t('branch'));
    return FALSE;
  }
  if (!_versioncontrol_has_account_access($branch, t('branch'))) {
    return FALSE;
  }

  return _versioncontrol_has_branch_or_tag_access(
    'versioncontrol_branch_access', $branch, $branched_items
  );
}

/**
 * Determine if a tag may be assigned to a set of items.
 * Call this inside a pre-commit/pre-tag hook.
 *
 * @param $tag
 *   A structured array that consists of the following elements:
 *
 *   - 'tag_name': The name of the tag (a string like 'DRUPAL-6--1-1').
 *   - 'action': Specifies what is going to happen with the tag. This is
 *        VERSIONCONTROL_ACTION_ADDED if the tag is being created,
 *        VERSIONCONTROL_ACTION_MODIFIED if it's being renamed,
 *        or VERSIONCONTROL_ACTION_DELETED if it is slated for deletion.
 *   - 'uid': The Drupal user id of the committer. Passing this is optional -
 *        if it isn't set, this function will determine the uid.
 *   - 'username': The system specific VCS username of the committer.
 *   - 'repository': The repository where the tagging occurs, given as a
 *        structured array, like a single element of what is returned
 *        by versioncontrol_get_repositories().
 *        You can either pass this or 'repo_id'.
 *   - 'repo_id': The repository where the tagging occurs, given as a simple
 *        integer id. You can either pass this or 'repository'.
 *   - 'message': The tag message that the user has given. If the version
 *        control system doesn't support tag messages, leave 'message' unset.
 *   - '[xxx]_specific': An array of VCS specific additional tag operation
 *        info. How this array looks like is defined by the corresponding
 *        backend module (versioncontrol_[xxx]).
 *
 * @param $tagged_items
 *   An array of all items that are affected by the tagging operation.
 *   Compared to standard item arrays, the ones in here may not have the
 *   'revision' element set and can optionally contain a 'source branch'
 *   element that specifies the original branch of this item.
 *   (For $op == 'move' or $op == 'delete', 'source branch' is never set.)
 *   An empty $tagged_items array means that the whole repository has been
 *   tagged.
 *
 * @return
 *   TRUE if the tag may be assigned, or FALSE if not.
 *   If FALSE is returned, you can retrieve the concerning error messages
 *   by calling versioncontrol_get_access_errors().
 */
function versioncontrol_has_tag_access($tag, $tagged_items) {
  $tag = _versioncontrol_fill_branch_or_tag($tag, $tagged_items);
  if (!isset($tag['repository'])) {
    _versioncontrol_set_repository_error(t('tag'));
    return FALSE;
  }
  if (!_versioncontrol_has_account_access($tag, t('tag'))) {
    return FALSE;
  }

  if (!isset($tag['message'])) {
    $tag['message'] = '';
  }
  else if (empty($tag['message'])) {
    // If we let empty messages through, we can't tell if the backend doesn't
    // support tag messages, or if the user provided one. So, disallow empty
    // tag messages by the user, as that's bad practice anyways.
    _versioncontrol_access_errors(array(
      t('** ERROR: You have to provide a tag message.'),
    ));
    return FALSE;
    // So, that's that. Now if empty($tag['message']), it can always be assumed
    // that the VCS doesn't support tag messages.
  }

  return _versioncontrol_has_branch_or_tag_access(
    'versioncontrol_tag_access', $tag, $tagged_items
  );
}

/**
 * The branch_access() and tag_access() functions work totally similar
 * internally, so let's share the code in a common function.
 */
function _versioncontrol_has_branch_or_tag_access($hook, $branch_or_tag, $items) {
  $error_messages = array();

  foreach (module_implements($hook) as $module) {
    $function = $module .'_'. $hook;

    // If at least one access hook returns TRUE, the branch/tag goes through.
    $outcome = $function($branch_or_tag, $items);
    if ($outcome === TRUE) {
      return TRUE;
    }
    else { // If !TRUE, $outcome is required to be an array with error messages
      $error_messages = array_merge($error_messages, $outcome);
    }
  }

  // Let the branch/tag assignment fail if there's more than zero error messages.
  if (!empty($error_messages)) {
    _versioncontrol_access_errors($error_messages);
    return FALSE;
  }
  return TRUE;
}

/**
 * If versioncontrol_has_commit_access(), versioncontrol_has_branch_access()
 * or versioncontrol_has_tag_access() returned FALSE, you can use this function
 * to retrieve the list of error messages from the various access checks.
 * The error messages do not include trailing linebreaks, it is expected that
 * those are inserted by the caller.
 */
function versioncontrol_get_access_errors() {
  return _versioncontrol_access_errors();
}

/**
 * Retrieve or set the list of access errors.
 */
function _versioncontrol_access_errors($new_messages = NULL) {
  static $error_messages = array();

  if (isset($new_messages)) {
    $error_messages = $new_messages;
  }
  return $error_messages;
}

/**
 * Insert a commit into the database, and call the necessary module hooks.
 * Only call this function after the commit has been successfully executed.
 *
 * @param $commit
 *   A single commit array like the ones returned by
 *   versioncontrol_get_commits(), but leaving out on a few details that
 *   will instead be determined by this function. Here's the allowed elements:
 *
 *   - 'repository': The repository where this commit occurred, given as a
 *        structured array, like a single element of what is returned
 *        by versioncontrol_get_repositories().
 *        You can either pass this or 'repo_id'.
 *   - 'repo_id': The repository where this commit occurred, given as a simple
 *        integer id. You can either pass this or 'repository'.
 *   - 'date': The time when the revision was committed, given as Unix timestamp.
 *   - 'uid': The Drupal user id of the committer. Passing this is optional -
 *        if it isn't set, this function will determine the uid.
 *   - 'username': The system specific VCS username of the committer.
 *   - 'message': The commit message.
 *   - 'revision': The VCS specific repository-wide revision identifier,
 *        like '' in CVS, '27491' in Subversion or some SHA-1 key in various
 *        distributed version control systems. If there is no such revision
 *        (which may be the case for version control systems that don't support
 *        atomic commits) then the 'revision' element is an empty string.
 *   - '[xxx]_specific': An array of VCS specific additional commit information.
 *        How this array looks like is defined by the corresponding
 *        backend module (versioncontrol_[xxx]). If the backend has registered
 *        itself with the VERSIONCONTROL_FLAG_AUTOADD_COMMITS option, all items
 *        of this array will automatically be inserted into the
 *        {versioncontrol_[xxx]_commits} table.
 *
 * @param $commit_actions
 *   A structured array containing the exact details of what happened to
 *   each item in this commit. The structure of this array is the same as
 *   the return value of versioncontrol_get_commit_actions().
 *
 * @return
 *   The finalized commit array, with all of the 'commit_id', 'repository',
 *   'uid' and 'directory' properties filled in, and 'repo_id' removed if it
 *   existed before.
 *   In case of an error, NULL is returned instead of the commit array.
 */
function versioncontrol_insert_commit($commit, $commit_actions) {
  $commit = _versioncontrol_fill_commit($commit, $commit_actions, TRUE);
  if (!isset($commit['repository'])) {
    return NULL;
  }

  // Ok, everything's there, insert the commit into the database.
  $commit['commit_id'] = db_next_id('{versioncontrol_commits}_commit_id');

  db_query(
    "INSERT INTO {versioncontrol_commits}
     (commit_id, repo_id, date, uid, username, directory, message, revision)
     VALUES (%d, %d, %d, %d, '%s', '%s', '%s', '%s')",
    $commit['commit_id'], $commit['repository']['repo_id'],
    $commit['date'], $commit['uid'], $commit['username'], $commit['directory'],
    $commit['message'], $commit['revision']
  );

  // Auto-add commit info from $commit['[xxx]_specific'] into the database.
  $backends = versioncontrol_get_backends();
  $vcs = $commit['repository']['vcs'];
  $is_autoadd = in_array(VERSIONCONTROL_FLAG_AUTOADD_COMMITS,
                         $backends[$vcs]['flags']);
  if ($is_autoadd) {
    $table_name = 'versioncontrol_'. $vcs .'_commits';
    $elements = $commit[$vcs .'_specific'];
    $elements['commit_id'] = $commit['commit_id'];
    _versioncontrol_db_insert_additions($table_name, $elements);
  }
  // Provide an opportunity for the backend to add its own stuff.
  if (versioncontrol_backend_implements($vcs, 'commit')) {
    _versioncontrol_call_backend($vcs, 'commit',
                                 array('insert', $commit, $commit_actions));
  }

  // Everything's done, let the world know about it!
  module_invoke_all('versioncontrol_commit', 'insert', $commit, $commit_actions);

  return $commit;
}

/**
 * Insert a branch operation into the database, and call the necessary hooks.
 *
 * @param $branch
 *   A structured array that consists of the following elements:
 *
 *   - 'branch_name': The name of the target branch
 *        (a string like 'DRUPAL-6--1').
 *   - 'action': Specifies what happened to the branch. This is
 *        VERSIONCONTROL_ACTION_ADDED if the branch was created,
 *        VERSIONCONTROL_ACTION_MODIFIED if was renamed,
 *        or VERSIONCONTROL_ACTION_DELETED if was deleted.
 *   - 'date': The time when the branching was done, given as Unix timestamp.
 *   - 'uid': The Drupal user id of the committer. Passing this is optional -
 *        if it isn't set, this function will determine the uid.
 *   - 'username': The system specific VCS username of the committer.
 *   - 'repository': The repository where the branching occurred,
 *        given as a structured array, like the return value
 *        of versioncontrol_get_repository().
 *        You can either pass this or 'repo_id'.
 *   - 'repo_id': The repository where the branching occurs, given as a simple
 *        integer id. You can either pass this or 'repository'.
 *   - '[xxx]_specific': An array of VCS specific additional branch operation
 *        info. How this array looks like is defined by the corresponding
 *        backend module (versioncontrol_[xxx]). If the backend has registered
 *        itself with the VERSIONCONTROL_FLAG_AUTOADD_BRANCH_OPERATIONS option,
 *        all items of this array will automatically be inserted into the
 *        {versioncontrol_[xxx]_branch_operations} table.
 *
 * @param $branched_items
 *   An array of all items that are affected by the branching operation.
 *   Compared to standard item arrays, the ones in here can optionally contain
 *   a 'source branch' element that specifies the original branch name
 *   of this item. (For $op == 'delete', 'source branch' is never set.)
 *   Note that the 'revision' element - which is optional for
 *   versioncontrol_has_branch_access() - is compulsory for items in here.
 *   An empty $branched_items array means that the whole repository has been
 *   branched.
 *
 * @return
 *   The finalized branch operation array, with all of the 'branch_op_id',
 *   'repository', 'uid' and 'directory' properties filled in, and 'repo_id'
 *   removed if it existed before. In case of an error, NULL is returned
 *   instead of the branch operation array.
 */
function versioncontrol_insert_branch_operation($branch, $branched_items) {
  $branch = _versioncontrol_fill_branch_or_tag($branch, $branched_items);
  if (!isset($branch['repository'])) {
    return NULL;
  }
  $branch_id = versioncontrol_ensure_branch(
    $branch['branch_name'], $branch['repository']['repo_id']
  );

  // Ok, everything's there, insert the branch into the database.
  $branch['branch_op_id'] = db_next_id('{versioncontrol_branch_operations}_branch_op_id');

  db_query("INSERT INTO {versioncontrol_branch_operations}
            (branch_op_id, branch_id, action, date, uid, username, directory)
            VALUES (%d, %d, %d, %d, %d, '%s', '%s')",
            $branch['branch_op_id'], $branch_id, $branch['action'],
            $branch['date'], $branch['uid'], $branch['username'],
            $branch['directory']);

  // Auto-add branch info from $branch['[xxx]_specific'] into the database.
  $backends = versioncontrol_get_backends();
  $is_autoadd = in_array(VERSIONCONTROL_FLAG_AUTOADD_BRANCH_OPERATIONS,
                         $backends[$branch['repository']['vcs']]['flags']);

  _versioncontrol_insert_branch_or_tag_operation(
    $branch, $branched_items, 'branch', $is_autoadd
  );
  return $branch;
}

/**
 * Insert a tag operation into the database, and call the necessary hooks.
 *
 * @param $tag
 *   A structured array that consists of the following elements:
 *
 *   - 'tag_name': The name of the tag (a string like 'DRUPAL-6--1-1').
 *   - 'action': Specifies what happened to the tag. This is
 *        VERSIONCONTROL_ACTION_ADDED if the tag was created,
 *        VERSIONCONTROL_ACTION_MOVED if was renamed,
 *        or VERSIONCONTROL_ACTION_DELETED if was deleted.
 *   - 'date': The time when the tagging was done, given as Unix timestamp.
 *   - 'uid': The Drupal user id of the committer. Passing this is optional -
 *        if it isn't set, this function will determine the uid.
 *   - 'username': The system specific VCS username of the committer.
 *   - 'repository': The repository where the tagging occurred,
 *        given as a structured array, like the return value
 *        of versioncontrol_get_repository().
 *        You can either pass this or 'repo_id'.
 *   - 'repo_id': The repository where the tagging occurs, given as a simple
 *        integer id. You can either pass this or 'repository'.
 *   - 'message': The tag message that the user has given. If the version
 *        control system doesn't support tag messages, leave 'message' unset.
 *   - '[xxx]_specific': An array of VCS specific additional tag operation
 *        info. How this array looks like is defined by the corresponding
 *        backend module (versioncontrol_[xxx]). If the backend has registered
 *        itself with the VERSIONCONTROL_FLAG_AUTOADD_TAG_OPERATIONS option,
 *        all items of this array will automatically be inserted into the
 *        {versioncontrol_[xxx]_tag_operations} table.
 *
 * @param $tagged_items
 *   An array of all items that are affected by the tagging operation.
 *   Compared to standard item arrays, the ones in here can optionally contain
 *   a 'source branch' element that specifies the original branch name
 *   of this item. (For $op == 'move' or $op == 'delete', 'source branch' is
 *   never set.) Note that the 'revision' element - which is optional for
 *   versioncontrol_has_tag_access() - is compulsory for items in here.
 *   An empty $tagged_items array means that the whole repository has been
 *   tagged.
 *
 * @return
 *   The finalized tag operation array, with all of the 'tag_op_id',
 *   'repository', 'uid' and 'directory' properties filled in, and 'repo_id'
 *   removed if it existed before. In case of an error, NULL is returned
 *   instead of the tag operation array.
 */
function versioncontrol_insert_tag_operation($tag, $tagged_items) {
  $tag = _versioncontrol_fill_branch_or_tag($tag, $tagged_items);
  if (!isset($tag['repository'])) {
    return NULL;
  }
  if (!isset($tag['message'])) {
    $tag['message'] = '';
  }

  // Ok, everything's there, insert the tag into the database.
  $tag['tag_op_id'] = db_next_id('{versioncontrol_tag_operations}_tag_op_id');

  db_query("INSERT INTO {versioncontrol_tag_operations}
            (tag_op_id, repo_id, tag_name, action, date, uid, username,
             directory, message)
            VALUES (%d, %d, '%s', %d, %d, %d, '%s', '%s', '%s')",
            $tag['tag_op_id'], $tag['repository']['repo_id'], $tag['tag_name'],
            $tag['action'], $tag['date'], $tag['uid'], $tag['username'],
            $tag['directory'], $tag['message']);

  $backends = versioncontrol_get_backends();
  $is_autoadd = in_array(VERSIONCONTROL_FLAG_AUTOADD_TAG_OPERATIONS,
                         $backends[$tag['repository']['vcs']]['flags']);

  _versioncontrol_insert_branch_or_tag_operation(
    $tag, $tagged_items, 'tag', $is_autoadd
  );
  return $tag;
}

/**
 * Code that is shared between the branch and tag insertion functions.
 */
function _versioncontrol_insert_branch_or_tag_operation($branch_or_tag, $items, $type, $is_autoadd) {
  $vcs = $branch_or_tag['repository']['vcs'];

  // Auto-add additional info from $branch_or_tag['[xxx]_specific']
  // into the database.
  if ($is_autoadd) {
    $table_name = 'versioncontrol_'. $vcs .'_'. $type .'_operations';
    $elements = $branch_or_tag[$vcs .'_specific'];
    $elements[$type .'_op_id'] = $branch_or_tag[$type .'_op_id'];
    _versioncontrol_db_insert_additions($table_name, $elements);
  }

  // Provide an opportunity for the backend to add its own stuff.
  // Calls [xxx]_branch_operation() or [xxx]_tag_operation().
  if (versioncontrol_backend_implements($vcs, $type .'_operation')) {
    _versioncontrol_call_backend($vcs, $type .'_operation',
                                 array('insert', $branch_or_tag, $items));
  }

  // Everything's done, let the world know about it!
  // Calls hook_versioncontrol_branch_operation()
  // or hook_versioncontrol_tag_operation().
  module_invoke_all('versioncontrol_'. $type .'_operation',
                    'insert', $branch_or_tag, $items);
}

/**
 * Delete a commit from the database, and call the necessary hooks.
 *
 * @param $commit
 *   The commit array containing the commit that is to be deleted.
 *   It's a single commit array like one element in the return value of
 *   versioncontrol_get_commits().
 */
function versioncontrol_delete_commit($commit) {
  $commit_actions = versioncontrol_get_commit_actions($commit);

  // Announce deletion of the commit before anything has happened.
  module_invoke_all('versioncontrol_commit', 'delete', $commit, $commit_actions);

  // Provide an opportunity for the backend to delete its own stuff.
  if (versioncontrol_backend_implements($commit['repository']['vcs'], 'commit')) {
    _versioncontrol_call_backend($commit['repository']['vcs'], 'commit',
                                 array('delete', $commit, $commit_actions));
  }

  // Auto-delete commit info from $commit['[xxx]_specific'] from the database.
  $backends = versioncontrol_get_backends();
  if (isset($backends[$vcs])) { // not the case when called from uninstall
    $is_autoadd = in_array(VERSIONCONTROL_FLAG_AUTOADD_COMMITS,
                           $backends[$vcs]['flags']);
  }
  if ($is_autoadd) {
    $table_name = 'versioncontrol_'. $vcs .'_commits';
    _versioncontrol_db_delete_additions($table_name, 'commit_id', $commit['commit_id']);
  }

  db_query('DELETE FROM {versioncontrol_commits}
            WHERE commit_id = %d', $commit['commit_id']);
}

/**
 * Delete a branch operation from the database, and call the necessary hooks.
 *
 * @param $branch
 *   The array containing the branch operation that is to be deleted.
 *   It's a single branch operation array like the return value
 *   of versioncontrol_get_branch_operation().
 */
function versioncontrol_delete_branch_operation($branch) {
  $branched_items = versioncontrol_get_branched_items($branch);

  _versioncontrol_delete_branch_or_tag_operation(
    $branch, $branched_items, 'branch', VERSIONCONTROL_FLAG_AUTOADD_BRANCH_OPERATIONS
  );
}

/**
 * Delete a tag operation from the database, and call the necessary hooks.
 *
 * @param $tag
 *   The array containing the tag operation that is to be deleted.
 *   It's a single tag operation array like one the return value
 *   of versioncontrol_get_tag_operation().
 */
function versioncontrol_delete_tag_operation($tag) {
  $tagged_items = versioncontrol_get_tagged_items($tag);

  _versioncontrol_delete_branch_or_tag_operation(
    $tag, $tagged_items, 'tag', VERSIONCONTROL_FLAG_AUTOADD_TAG_OPERATIONS
  );
}

/**
 * Code that is shared between the branch and tag deletion functions.
 */
function _versioncontrol_delete_branch_or_tag_operation($branch_or_tag, $items, $type, $autoadd_flag) {
  // Announce deletion of the commit before anything has happened.
  // Calls hook_versioncontrol_branch_operation()
  // or hook_versioncontrol_tag_operation().
  module_invoke_all('versioncontrol_'. $type .'_operation',
                    'delete', $branch_or_tag, $items);

  $vcs = $branch_or_tag['repository']['vcs'];

  // Provide an opportunity for the backend to delete its own stuff.
  // Calls [xxx]_branch_operation() or [xxx]_tag_operation().
  if (versioncontrol_backend_implements($vcs, $type .'_operation')) {
    _versioncontrol_call_backend($vcs, $type .'_operation',
                                 array('delete', $branch_or_tag, $items));
  }

  // Auto-delete additional tag info from
  // $branch_or_tag['repository']['[xxx]_specific'] from the database.
  $backends = versioncontrol_get_backends();
  if (isset($backends[$vcs])) { // not the case when called from uninstall
    $is_autoadd = in_array($autoadd_flag, $backends[$vcs]['flags']);
  }
  if ($is_autoadd) {
    $table_name = 'versioncontrol_'. $vcs .'_'. $type .'_operations';
    _versioncontrol_db_delete_additions(
      $table_name, $type .'_op_id', $branch_or_tag[$type .'_op_id']
    );
  }

  db_query('DELETE FROM {versioncontrol_'. $type .'_operations}
            WHERE '. $type .'_op_id = %d', $branch_or_tag[$type .'_op_id']);
}


/**
 * Code that is shared between versioncontrol_has_commit_access()
 * and versioncontrol_insert_commit(): Fill up a commit array's missing values.
 *
 * @return
 *   The completed commit array. Check on isset($commit['repository'])
 *   before proceeding.
 */
function _versioncontrol_fill_commit($commit, $commit_actions, $include_unauthorized = FALSE) {
  $commit = _versioncontrol_fill_uid_and_repository($commit, $include_unauthorized);
  $paths = _versioncontrol_get_commit_action_paths($commit_actions);
  $commit['directory'] = _versioncontrol_get_common_directory($paths);
  return $commit;
}

/**
 * Code that is shared between versioncontrol_has_{branch,tag}_access()
 * and versioncontrol_insert_{branch,tag}_operation():
 * Fill up a branch/tag operation array's missing values.
 *
 * @return
 *   The completed branch/tag array.
 *   Check on isset($branch_or_tag['repository']) before proceeding.
 */
function _versioncontrol_fill_branch_or_tag($branch_or_tag, $items) {
  $branch_or_tag = _versioncontrol_fill_uid_and_repository($branch_or_tag);
  $paths = _versioncontrol_get_item_paths($items);
  $branch_or_tag['directory'] = _versioncontrol_get_common_directory($paths);
  return $branch_or_tag;
}

function _versioncontrol_get_commit_action_paths($commit_actions) {
  $paths = array();

  // gather the paths of all current and source items
  foreach ($commit_actions as $path => $action) {
    if (isset($action['current item'])) {
      $paths[] = $action['current item']['path'];
    }
    // Don't get items twice that were only modified have the same path.
    if ($action['action'] != VERSIONCONTROL_ACTION_MODIFIED && isset($action['source items'])) {
      foreach ($action['source items'] as $item) {
        $paths[] = $item['path'];
      }
    }
  }
  return $paths;
}

function _versioncontrol_get_item_paths($items) {
  $paths = array();
  foreach ($items as $item) {
    $paths[] = $item['path'];
  }
  return $paths;
}

/**
 * Code that is shared between _versioncontrol_fill_commit() and the
 * branch/tag access functions: fill up Drupal user id and repository
 * in case they are not given.
 */
function _versioncontrol_fill_uid_and_repository($object, $include_unauthorized = FALSE) {
  // If not already there, retrieve the full repository object.
  if (!isset($object['repository']) && isset($object['repo_id'])) {
    $object['repository'] = versioncontrol_get_repository($object['repo_id']);
    unset($object['repo_id']);
  }

  // If not already there, retrieve the Drupal user id of the committer.
  if (!isset($object['uid'])) {
    $uid = versioncontrol_get_account_uid_for_username(
      $object['repository']['repo_id'], $object['username'], $include_unauthorized
    );
    // If no uid could be retrieved, blame the commit on user 0 (anonymous).
    $object['uid'] = isset($uid) ? $uid : 0;
  }
  return $object;
}

/**
 * Retrieve the deepest-level directory in the repository that is common
 * to all the given paths, e.g. '/src' if the $paths argument contains
 * '/src/subdir/code.php' and '/src/README.txt', or '/' if it contains
 * '/src/README.txt' and '/doc'.
 *
 * @param $paths
 *   The paths of all items of which the common directory should be retrieved.
 *
 * @return
 *   The common directory path of all items in the $commit_actions array,
 *   as mentioned above.
 */
function _versioncontrol_get_common_directory($paths) {
  if (empty($paths)) {
    return '/';
  }

  $dirparts = explode('/', dirname(array_pop($paths)));

  foreach ($paths as $path) {
    $new_dirparts = array();
    $current_dirparts = explode('/', dirname($path));
    $mincount = min(count($dirparts), count($current_dirparts));

    for ($i = 0; $i < $mincount; $i++) {
      if ($dirparts[$i] == $current_dirparts[$i]) {
        $new_dirparts[] = $dirparts[$i];
      }
    }
    $dirparts = $new_dirparts;
  }

  if (count($dirparts) == 1) {
    return '/';
  }
  return implode('/', $dirparts);
}


/**
 * Insert a VCS user account into the database,
 * and call the necessary module hooks.
 *
 * @param $repository
 *   The repository where the user has its VCS account.
 * @param $uid
 *   The Drupal user id corresponding to the VCS username.
 * @param $username
 *   The VCS specific username (a string).
 * @param $additional_data
 *   An array of additional author information. Modules can fill this array
 *   by implementing hook_versioncontrol_extract_account_data().
 */
function versioncontrol_insert_account($repository, $uid, $username, $additional_data = array()) {
  db_query(
    "INSERT INTO {versioncontrol_accounts} (uid, repo_id, username)
     VALUES (%d, %d, '%s')", $uid, $repository['repo_id'], $username
  );

  // Provide an opportunity for the backend to add its own stuff.
  if (versioncontrol_backend_implements($repository['vcs'], 'account')) {
    _versioncontrol_call_backend(
      $repository['vcs'], 'account',
      array('insert', $uid, $username, $repository, $additional_data)
    );
  }

  // Update the commits table.
  db_query("UPDATE {versioncontrol_commits}
            SET uid = %d
            WHERE username = '%s' AND repo_id = %d",
            $uid, $username, $repository['repo_id']);

  // Everything's done, let the world know about it!
  module_invoke_all('versioncontrol_account',
    'insert', $uid, $username, $repository, $additional_data
  );

  watchdog('special',
            'Version Control API: added '. $username .' account '.
            'in repository '. $repository['name'],
            l('view', 'admin/project/versioncontrol-accounts'));
}

/**
 * Update a VCS user account in the database, and call the necessary
 * module hooks. The @p $repository and @p $uid parameters must stay the same
 * values as the one given on account creation, whereas @p $username and
 * @p $additional_data may change.
 *
 * @param $uid
 *   The Drupal user id corresponding to the VCS username.
 * @param $username
 *   The VCS specific username (a string).
 * @param $repository
 *   The repository where the user has its VCS account.
 * @param $additional_data
 *   An array of additional author information. Modules can fill this array
 *   by implementing hook_versioncontrol_extract_account_data().
 */
function versioncontrol_update_account($repository, $uid, $username, $additional_data = array()) {
  $old_username = versioncontrol_get_account_username_for_uid($repository['repo_id'], $uid, TRUE);
  $username_changed = ($username != $old_username);

  if ($username_changed) {
    db_query("UPDATE {versioncontrol_accounts}
              SET username = '%s'
              WHERE uid = %d AND repo_id = %d",
              $username, $uid, $repository['repo_id']
    );
  }

  // Provide an opportunity for the backend to add its own stuff.
  if (versioncontrol_backend_implements($repository['vcs'], 'account')) {
    _versioncontrol_call_backend(
      $repository['vcs'], 'account',
      array('update', $uid, $username, $repository, $additional_data)
    );
  }

  // Update the commits table.
  if ($username_changed) {
    db_query("UPDATE {versioncontrol_commits}
              SET uid = 0
              WHERE uid = %d AND repo_id = %d",
              $uid, $repository['repo_id']);
    db_query("UPDATE {versioncontrol_commits}
              SET uid = %d
              WHERE username = '%s' AND repo_id = %d",
              $uid, $username, $repository['repo_id']);
  }

  // Everything's done, let the world know about it!
  module_invoke_all('versioncontrol_account',
    'update', $uid, $username, $repository, $additional_data
  );

  watchdog('special',
            'Version Control API: updated '. $username .' account '.
            'in repository '. $repository['name'],
            l('view', 'admin/project/versioncontrol-accounts'));
}

/**
 * Delete a VCS user account from the database, set all commits with this
 * account as author to user 0 (anonymous), and call the necessary hooks.
 *
 * @param $repository
 *   The repository where the user has its VCS account.
 * @param $uid
 *   The Drupal user id corresponding to the VCS username.
 * @param $username
 *   The VCS specific username (a string).
 */
function versioncontrol_delete_account($repository, $uid, $username) {
  // Update the commits table.
  db_query('UPDATE {versioncontrol_commits}
            SET uid = 0
            WHERE uid = %d AND repo_id = %d',
            $uid, $repository['repo_id']);

  // Announce deletion of the account before anything has happened.
  module_invoke_all('versioncontrol_account',
    'delete', $uid, $username, $repository, array()
  );

  // Provide an opportunity for the backend to delete its own stuff.
  if (versioncontrol_backend_implements($repository['vcs'], 'account')) {
    _versioncontrol_call_backend(
      $repository['vcs'], 'account',
      array('delete', $uid, $username, $repository, array())
    );
  }

  db_query('DELETE FROM {versioncontrol_accounts}
            WHERE uid = %d AND repo_id = %d',
            $uid, $repository['repo_id']);

  watchdog('special',
            'Version Control API: deleted '. $username .' account '.
            'in repository '. $repository['name'],
            l('view', 'admin/project/versioncontrol-accounts'));
}

/**
 * Insert a repository into the database, and call the necessary hooks.
 *
 * @param $repository
 *   The repository array containing the new or existing repository.
 *   It's a single repository array like the one returned by
 *   versioncontrol_get_repository(), so it consists of the following elements:
 *
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
 *        If the backend has registered itself with the
 *        VERSIONCONTROL_FLAG_AUTOADD_REPOSITORIES option, all items of
 *        this array will automatically be inserted into the
 *        {versioncontrol_[xxx]_commits} table.
 *
 * @param $repository_urls
 *   An array of repository viewer URLs. How this array looks like is
 *   defined by the corresponding URL backend.
 *
 * @return
 *   The finalized repository array, including the 'repo_id' element.
 */
function versioncontrol_insert_repository($repository, $repository_urls) {
  $repository['repo_id'] = db_next_id('{versioncontrol_repositories}_repo_id');

  db_query(
    "INSERT INTO {versioncontrol_repositories}
     (repo_id, name, vcs, root, authorization_method, url_backend)
     VALUES (%d, '%s', '%s', '%s', '%s', '%s')",
     $repository['repo_id'], $repository['name'], $repository['vcs'],
     $repository['root'], $repository['authorization_method'],
     $repository['url_backend']
  );
  // TODO: abstract out repository URLs into separate backends
  db_query(
    "INSERT INTO {versioncontrol_repository_urls}
     (repo_id, commit_view, file_log_view, file_view, directory_view, diff, tracker)
     VALUES (%d, '%s', '%s', '%s', '%s', '%s', '%s')",
     $repository['repo_id'], $repository_urls['commit_view'],
     $repository_urls['file_log_view'], $repository_urls['file_view'],
     $repository_urls['directory_view'], $repository_urls['diff'],
     $repository_urls['tracker']
  );

  // Auto-add repository info from $repository['[xxx]_specific'] into the database.
  $backends = versioncontrol_get_backends();
  $vcs = $repository['vcs'];
  $is_autoadd = in_array(VERSIONCONTROL_FLAG_AUTOADD_REPOSITORIES,
                         $backends[$vcs]['flags']);
  if ($is_autoadd) {
    $table_name = 'versioncontrol_'. $vcs .'_repositories';
    $elements = $repository[$vcs .'_specific'];
    $elements['repo_id'] = $repository['repo_id'];
    _versioncontrol_db_insert_additions($table_name, $elements);
  }

  // Provide an opportunity for the backend to add its own stuff.
  if (versioncontrol_backend_implements($vcs, 'repository')) {
    _versioncontrol_call_backend($vcs, 'repository', array('insert', $repository));
  }

  // Everything's done, let the world know about it!
  module_invoke_all('versioncontrol_repository', 'insert', $repository);

  watchdog('special',
            'Version Control API: added repository '. $repository['name'],
            l('view', 'admin/project/versioncontrol-repositories'));

  return $repository;
}

/**
 * Update a repository in the database, and call the necessary hooks.
 * The 'repo_id' and 'vcs' properties of the repository array must stay
 * the same as the ones given on repository creation,
 * whereas all other values may change.
 *
 * @param $repository
 *   The repository array containing the new or existing repository.
 *   It's a single repository array like the one returned by
 *   versioncontrol_get_repository(), so it consists of the following elements:
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
 *        If the backend has registered itself with the
 *        VERSIONCONTROL_FLAG_AUTOADD_REPOSITORIES option, all items of
 *        this array will automatically be inserted into the
 *        {versioncontrol_[xxx]_commits} table.
 *
 * @param $repository_urls
 *   An array of repository viewer URLs. How this array looks like is
 *   defined by the corresponding URL backend.
 */
function versioncontrol_update_repository($repository, $repository_urls) {
  db_query(
    "UPDATE {versioncontrol_repositories}
     SET name = '%s', vcs = '%s', root = '%s',
         authorization_method = '%s', url_backend = '%s'
     WHERE repo_id = %d",
     $repository['name'], $repository['vcs'], $repository['root'],
     $repository['authorization_method'], $repository['url_backend'],
     $repository['repo_id']
  );
  // TODO: abstract out repository URLs into separate backends
  db_query(
    "UPDATE {versioncontrol_repository_urls}
     SET commit_view = '%s', file_log_view = '%s', file_view = '%s',
         directory_view = '%s', diff = '%s', tracker = '%s'
     WHERE repo_id = %d",
     $repository_urls['commit_view'], $repository_urls['file_log_view'],
     $repository_urls['file_view'], $repository_urls['directory_view'],
     $repository_urls['diff'], $repository_urls['tracker'],
     $repository['repo_id']
  );

  // Auto-add commit info from $commit['[xxx]_specific'] into the database.
  $backends = versioncontrol_get_backends();
  $vcs = $repository['vcs'];
  $is_autoadd = in_array(VERSIONCONTROL_FLAG_AUTOADD_REPOSITORIES,
                         $backends[$vcs]['flags']);
  if ($is_autoadd) {
    $table_name = 'versioncontrol_'. $vcs .'_repositories';
    $elements = $repository[$vcs .'_specific'];
    $elements['repo_id'] = $repository['repo_id'];
    _versioncontrol_db_update_additions($table_name, 'repo_id', $elements);
  }

  // Provide an opportunity for the backend to add its own stuff.
  if (versioncontrol_backend_implements($vcs, 'repository')) {
    _versioncontrol_call_backend($vcs, 'repository', array('update', $repository));
  }

  // Everything's done, let the world know about it!
  module_invoke_all('versioncontrol_repository', 'update', $repository);

  watchdog('special',
            'Version Control API: updated repository '. $repository['name'],
            l('view', 'admin/project/versioncontrol-repositories'));
}

/**
 * Delete a repository from the database, and call the necessary hooks.
 * Together with the repository, all associated commits and accounts are
 * deleted as well.
 *
 * @param $repository
 *   The repository array containing the repository that is to be deleted.
 *   It's a single repository array like the one returned by
 *   versioncontrol_get_repository().
 */
function versioncontrol_delete_repository($repository) {
  // Delete tags.
  $tags = versioncontrol_get_tag_operations(array('repo_ids' => array($repository['repo_id'])));
  foreach ($tags as $tag) {
    versioncontrol_delete_tag_operation($tag);
  }
  // Delete branches.
  $branches = versioncontrol_get_branch_operations(array('repo_ids' => array($repository['repo_id'])));
  foreach ($branches as $branch) {
    versioncontrol_delete_branch_operation($branch);
  }
  db_query('DELETE FROM {versioncontrol_branches}
            WHERE repo_id = %d', $repository['repo_id']);

  // Delete commits.
  $commits = versioncontrol_get_commits(array('repo_ids' => array($repository['repo_id'])));
  foreach ($commits as $commit) {
    versioncontrol_delete_commit($commit);
  }

  // Delete accounts.
  $accounts = versioncontrol_get_accounts(
    array('repo_ids' => array($repository['repo_id'])), TRUE
  );
  foreach ($accounts as $uid => $usernames_by_repository) {
    foreach ($usernames_by_repository as $repo_id => $username) {
      versioncontrol_delete_account($repository, $uid, $username);
    }
  }

  // Announce deletion of the repository before anything has happened.
  module_invoke_all('versioncontrol_repository', 'delete', $repository);

  $vcs = $repository['vcs'];

  // Provide an opportunity for the backend to delete its own stuff.
  if (versioncontrol_backend_implements($vcs, 'repository')) {
    _versioncontrol_call_backend($vcs, 'repository', array('delete', $repository));
  }

  // Auto-delete repository info from $repository['[xxx]_specific'] from the database.
  $backends = versioncontrol_get_backends();
  if (isset($backends[$vcs])) { // not the case when called from uninstall
    $is_autoadd = in_array(VERSIONCONTROL_FLAG_AUTOADD_REPOSITORIES,
                           $backends[$vcs]['flags']);
  }
  if ($is_autoadd) {
    $table_name = 'versioncontrol_'. $vcs .'_repositories';
    _versioncontrol_db_delete_additions($table_name, 'repo_id', $repository['repo_id']);
  }

  // Phew, everything's cleaned up. Finally, delete the repository.
  db_query('DELETE FROM {versioncontrol_repositories} WHERE repo_id = %d',
           $repository['repo_id']);
  // TODO: abstract out repository URLs into separate backends
  db_query('DELETE FROM {versioncontrol_repository_urls} WHERE repo_id = %d',
           $repository['repo_id']);
  db_query('DELETE FROM {versioncontrol_repository_metadata} WHERE repo_id = %d',
           $repository['repo_id']);

  watchdog('special',
            'Version Control API: deleted repository '. $repository['name'],
            l('view', 'admin/project/versioncontrol-repositories'));
}


/**
 * Retrieve the branch name and repository for a given branch id.
 *
 * @return
 *   A structured array consisting of the 'branch_id, 'repo_id' and
 *   'branch_name' elements, or NULL if there is no branch for this branch id.
 */
function versioncontrol_get_branch($branch_id) {
  static $branch_cache = array();

  if (isset($branch_cache[$branch_id])) {
    if ($branch_cache[$branch_id] === FALSE) {
      return NULL; // there's no branch for this id
    }
    return $branch_cache[$branch_id]; // return the cached branch array
  }

  $result = db_query('SELECT branch_id, repo_id, branch_name
                      FROM {versioncontrol_branches}
                      WHERE branch_id = %d', $branch_id);

  while ($branch = db_fetch_array($result)) {
    $branch_cache[$branch_id] = $branch;
    return $branch;
  }
  $branch_cache[$branch_id] = FALSE;
  return NULL;
}

/**
 * Retrieve the branch id for a given branch name from a specific repository.
 *
 * @return
 *   The branch id of the requested branch, or NULL if there is no branch
 *   for this name and repository.
 */
function versioncontrol_get_branch_id($branch_name, $repo_id) {
  static $branch_cache = array();

  if (isset($branch_cache[$repo_id][$branch_name])) {
    if ($branch_cache[$repo_id][$branch_name] === FALSE) {
      return NULL; // there's no branch for this name and repository
    }
    return $branch_cache[$repo_id][$branch_name]; // return the cached branch id
  }

  $result = db_query("SELECT branch_id
                      FROM {versioncontrol_branches}
                      WHERE repo_id = %d AND branch_name = '%s'",
                      $repo_id, $branch_name);

  while ($branch = db_fetch_object($result)) {
    $branch_cache[$repo_id][$branch_name] = $branch->branch_id;
    return $branch->branch_id;
  }
  $branch_cache[$repo_id][$branch_name] = FALSE;
  return NULL;
}

/**
 * Retrieve the branch id for a given branch name from a specific repository,
 * and create a new branch entry if no such branch exists yet.
 *
 * @return
 *   The branch id of the requested branch.
 */
function versioncontrol_ensure_branch($branch_name, $repo_id) {
  $branch_id = versioncontrol_get_branch_id($branch_name, $repo_id);
  if (!isset($branch_id)) {
    $branch_id = db_next_id('{versioncontrol_branches}_branch_id');
    db_query("INSERT INTO {versioncontrol_branches} (branch_id, repo_id, branch_name)
              VALUES (%d, %d, '%s')", $branch_id, $repo_id, $branch_name);
  }
  return $branch_id;
}

/**
 * Export a repository's authenticated accounts to the version control system's
 * password file format.
 *
 * @param $repository
 *   The repository array of the repository whose accounts should be exported.
 *
 * @return
 *   The plaintext result data which could be written into the password file
 *   as is.
 */
function versioncontrol_export_accounts($repository) {
  $accounts = versioncontrol_get_accounts(array(
    'repo_ids' => array($repository['repo_id']),
  ));
  return _versioncontrol_call_backend($repository['vcs'], 'export_accounts',
                                      array($repository, $accounts));
}


/**
 * Generate and execute an INSERT query for the given table based on key names,
 * values and types of the given array elements. This function basically
 * accomplishes the insertion part of Version Control API's 'autoadd' feature.
 */
function _versioncontrol_db_insert_additions($table_name, $elements) {
  $keys = array();
  $params = array();
  $types = array();

  foreach ($elements as $key => $value) {
    $keys[] = $key;
    $params[] = is_numeric($value) ? $value : serialize($value);
    $types[] = is_numeric($value) ? '%d' : "'%s'";
  }

  db_query(
    'INSERT INTO {'. $table_name .'} ('. implode(', ', $keys) .')
     VALUES ('. implode(', ', $types) .')', $params
  );
}

/**
 * Generate and execute an UPDATE query for the given table based on key names,
 * values and types of the given array elements. This function basically
 * accomplishes the update part of Version Control API's 'autoadd' feature.
 * In order to avoid unnecessary complexity, the primary key may not consist
 * of multiple columns and has to be a numeric value.
 */
function _versioncontrol_db_update_additions($table_name, $primary_key_name, $elements) {
  $set_statements = array();
  $params = array();

  foreach ($elements as $key => $value) {
    if ($key == $primary_key_name) {
      continue;
    }
    $type = is_numeric($value) ? '%d' : "'%s'";
    $set_statements[] = $key .' = '. $type;
    $params[] = is_numeric($value) ? $value : serialize($value);
  }
  $params[] = $elements[$primary_key_name];

  db_query(
    'UPDATE {'. $table_name .'}
     SET '. implode(', ', $set_statements) .'
     WHERE '. $primary_key_name .' = %d', $params
  );
}

/**
 * Generate and execute a DELETE query for the given table
 * based on name and value of the primary key.
 * In order to avoid unnecessary complexity, the primary key may not consist
 * of multiple columns and has to be a numeric value.
 */
function _versioncontrol_db_delete_additions($table_name, $primary_key_name, $primary_key) {
  db_query('DELETE FROM {'. $table_name .'}
            WHERE '. $primary_key_name .' = %d', $primary_key);
}
