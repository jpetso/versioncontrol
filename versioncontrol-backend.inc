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
 * Determine if a commit may be executed or not.
 * Call this inside a pre-commit hook.
 *
 * @param $commit
 *   A single commit array like the ones returned by
 *   versioncontrol_get_commits(), but leaving out on a few details that
 *   will instead be determined by this function. This array describes
 *   the commit that is about to happen. Here's the allowed elements:
 *
 *   'repository': The repository where this commit occurs, given as a
 *                 structured array, like a single element of what is returned
 *                 by versioncontrol_get_repositories().
 *                 You can either pass this or 'repo_id'.
 *   'repo_id': The repository where this commit occurs, given as a simple
 *              integer id. You can either pass this or 'repository'.
 *   'uid': The Drupal user id of the committer. Passing this is optional -
 *          if it isn't set, this function will determine the uid.
 *   'username': The system specific VCS username of the committer.
 *   'message': The commit message. Passing this is optional, just leave the
 *              'message' element unset if the VCS can't retrieve the commit
 *              message at the time of the pre-commit hook.
 *   '[xxx]_specific': An array of VCS specific additional commit information.
 *                     How this array looks like is defined by the
 *                     corresponding backend module (versioncontrol_[xxx]).
 *
 * @param $commit_actions
 *   A structured array containing the exact details of what is about to happen
 *   to each item in this commit. The structure of this array is the same as
 *   the return value of versioncontrol_get_commit_actions().
 *   The 'source items' element of each action and the 'revision' element of
 *   each item in these actions are optional and may be left unset.
 * @param $branch
 *   The target branch where the commit will happen (a string like 'DRUPAL-5').
 *   If the VCS can't retrieve the branch at the time of the pre-commit hook
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
    return NULL;
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
 * Determine if a branch may be assigned to a directory.
 * Call this inside a pre-commit/pre-branch hook.
 *
 * @param $op
 *   'add' if the branch is being created, 'move' if it's being renamed,
 *   or 'delete' if it is slated for deletion.
 *
 * @param $branch
 *   A structured array that consists of the following elements:
 *
 *   'name': The name of the target branch (a string like 'DRUPAL-6--1').
 *   'uid': The Drupal user id of the committer. Passing this is optional -
 *          if it isn't set, this function will determine the uid.
 *   'username': The system specific VCS username of the committer.
 *   'repository': The repository where the branching occurs, given as a
 *                 structured array, like a single element of what is returned
 *                 by versioncontrol_get_repositories().
 *                 You can either pass this or 'repo_id'.
 *   'repo_id': The repository where the branching occurs, given as a simple
 *              integer id. You can either pass this or 'repository'.
 *   'directory': The path of the (target) directory that is being branched.
 *   'items': If the backend can determine all items (including the item
 *            that corresponds to the 'directory' element) that are affected by
 *            the branching operation, they will be given as a structured array
 *            in this element. Array keys are the item paths, and array values
 *            are item arrays. Compared to standard item arrays, the ones
 *            in here don't need the 'revision' element to be set and can
 *            optionally contain a 'source branch' element that specifies
 *            the original branch of this item.
 *            (For $op == 'delete', 'source branch' is never set.)
 *            If the backend cannot determine all affected items,
 *            the 'items' element is not set.
 *
 * @return
 *   TRUE if the branch may be assigned, or FALSE if not.
 *   If FALSE is returned, you can retrieve the concerning error messages
 *   by calling versioncontrol_get_access_errors().
 */
function versioncontrol_has_branch_access($op, $branch) {
  return _versioncontrol_has_branch_or_tag_access(
    'versioncontrol_branch_access', $op, $branch
  );
}

/**
 * Determine if a tag may be assigned to a directory.
 * Call this inside a pre-commit/pre-tag hook.
 *
 * @param $op
 *   'add' if the tag is being created, 'move' if it's being renamed,
 *   or 'delete' if it is slated for deletion.
 *
 * @param $tag
 *   A structured array that consists of the following elements:
 *
 *   'name': The name of the tag (a string like 'DRUPAL-6--1-1').
 *   'uid': The Drupal user id of the committer. Passing this is optional -
 *          if it isn't set, this function will determine the uid.
 *   'username': The system specific VCS username of the committer.
 *   'repository': The repository where the tagging occurs, given as a
 *                 structured array, like a single element of what is returned
 *                 by versioncontrol_get_repositories().
 *                 You can either pass this or 'repo_id'.
 *   'repo_id': The repository where the tagging occurs, given as a simple
 *              integer id. You can either pass this or 'repository'.
 *   'directory': The path of the directory that is being tagged.
 *   'items': If the backend can determine all items (including the item
 *            that corresponds to the 'directory' element) that are affected by
 *            the tagging operation, they will be given as a structured array
 *            in this element. Array keys are the item paths, and array values
 *            are item arrays. Compared to standard item arrays, the ones
 *            in here don't need the 'revision' element to be set and can
 *            optionally contain a 'source branch' element that specifies
 *            the original branch of this item. (For $op == 'move' or
 *            $op == 'delete', 'source branch' is never set.)
 *            If the backend cannot determine all affected items,
 *            the 'items' element is not set.
 *
 * @return
 *   TRUE if the tag may be assigned, or FALSE if not.
 *   If FALSE is returned, you can retrieve the concerning error messages
 *   by calling versioncontrol_get_access_errors().
 */
function versioncontrol_has_tag_access($op, $tag) {
  return _versioncontrol_has_branch_or_tag_access(
    'versioncontrol_tag_access', $op, $tag
  );
}

/**
 * The branch_access() and tag_access() functions work totally similar
 * internally, so let's share the code in a common function.
 */
function _versioncontrol_has_branch_or_tag_access($hook, $op, $branch_or_tag) {
  $branch_or_tag = _versioncontrol_fill_uid_and_repository($branch_or_tag);
  $error_messages = array();

  foreach (module_implements($hook) as $module) {
    $function = $module .'_'. $hook;

    // If at least one access hook returns TRUE, the branch/tag goes through.
    $outcome = $function($op, $branch_or_tag);
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
 *   'repository': The repository where this commit occurred, given as a
 *                 structured array, like a single element of what is returned
 *                 by versioncontrol_get_repositories().
 *                 You can either pass this or 'repo_id'.
 *   'repo_id': The repository where this commit occurred, given as a simple
 *              integer id. You can either pass this or 'repository'.
 *   'date': The time when the revision was committed, given as Unix timestamp.
 *   'uid': The Drupal user id of the committer. Passing this is optional -
 *          if it isn't set, this function will determine the uid.
 *   'username': The system specific VCS username of the committer.
 *   'message': The commit message.
 *   'revision': The VCS specific repository-wide revision identifier,
 *               like NULL in CVS, '27491' in Subversion or some SHA-1 key
 *               in various distributed version control systems.
 *   '[xxx]_specific': An array of VCS specific additional commit information.
 *                     How this array looks like is defined by the
 *                     corresponding backend module (versioncontrol_[xxx]).
 *                     If the backend has registered itself with the
 *                     VERSIONCONTROL_FLAG_AUTOADD_COMMITS option, all items
 *                     of this array will automatically be inserted into the
 *                     {versioncontrol_[xxx]_commits} table.
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
  $commit = _versioncontrol_fill_commit($commit, $commit_actions);
  if (!isset($commit['repository'])) {
    return NULL;
  }

  // Ok, everything's there, insert the commit into the database.
  $commit['commit_id'] = db_next_id('{versioncontrol_commits}_commit_id');

  db_query(
    "INSERT INTO {versioncontrol_commits}
     (commit_id, repo_id, date, uid, username, directory, message, revision)
     VALUES ('%d', '%d', '%d', '%d', '%s', '%s', '%s', '%s')",
    $commit['commit_id'], $commit['repository']['repo_id'],
    $commit['date'], $commit['uid'], $commit['username'], $commit['directory'],
    $commit['message'], $commit['revision']
  );

  // Auto-add commit info from $commit['[xxx]_specific'] into the database.
  $backends = versioncontrol_get_backends();
  $is_autoadd = in_array(VERSIONCONTROL_FLAG_AUTOADD_COMMITS,
                         $backends[$commit['repository']['vcs']]['flags']);
  if ($is_autoadd) {
    $table_name = 'versioncontrol_'. $commit['repository']['vcs'] .'_commits';
    $elements = $commit[$commit['repository']['vcs'] .'_specific'];
    $elements['commit_id'] = $commit['commit_id'];
    _versioncontrol_db_insert_additions($table_name, $elements);
  }

  // Provide an opportunity for the backend to add its own stuff.
  if (versioncontrol_backend_implements($commit['repository']['vcs'], 'commit')) {
    _versioncontrol_call_backend(
      $commit['repository']['vcs'], 'commit',
      array('insert', $commit, $commit_actions)
    );
  }

  // Everything's done, let the world know about it!
  module_invoke_all('versioncontrol_commit',
    'insert', $commit, $commit_actions
  );

  return $commit;
}

/**
 * Code that is shared between versioncontrol_has_commit_access()
 * and versioncontrol_insert_commit(): fill up a commit array's missing values.
 *
 * @return
 *   The completed commit array. Check on isset($commit['repository'])
 *   before proceeding.
 */
function _versioncontrol_fill_commit($commit, $commit_actions) {
  $commit = _versioncontrol_fill_uid_and_repository($commit);
  $commit['directory'] = _versioncontrol_get_common_directory($commit_actions);
  return $commit;
}

/**
 * Code that is shared between _versioncontrol_fill_commit() and the
 * branch/tag access functions: fill up Drupal user id and repository
 * in case they are not given.
 */
function _versioncontrol_fill_uid_and_repository($object) {
  // If not already there, retrieve the full repository object.
  if (!isset($object['repository'])) {
    $repository = versioncontrol_get_repository($object['repo_id']);
    unset($object['repo_id']);
    if (isset($repository)) {
      $object['repository'] = $repository;
    }
  }

  // If not already there, retrieve the Drupal user id of the committer.
  if (!isset($object['uid'])) {
    $uid = versioncontrol_get_account_uid_for_username(
      $object['username'], $object['repository']['repo_id'], TRUE
    );
    // If no uid could be retrieved, blame the commit on user 0 (anonymous).
    $object['uid'] = isset($uid) ? $uid : 0;
  }
  return $object;
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
  module_invoke_all('versioncontrol_commit',
    'delete', $commit, $commit_actions
  );

  // Provide an opportunity for the backend to delete its own stuff.
  if (versioncontrol_backend_implements($repository['vcs'], 'commit')) {
    _versioncontrol_call_backend(
      $commit['repository']['vcs'], 'commit', array('delete', $commit, $commit_actions)
    );
  }

  db_query("DELETE FROM {versioncontrol_commits}
            WHERE commit_id = '%d'", $commit['commit_id']);
}

/**
 * Insert a VCS user account into the database,
 * and call the necessary module hooks.
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
function versioncontrol_insert_account($uid, $username, $repository, $additional_data = array()) {
  db_query(
    "INSERT INTO {versioncontrol_accounts} (uid, repo_id, username)
     VALUES ('%d', '%d', '%s')", $uid, $repository['repo_id'], $username
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
            SET uid = '%d'
            WHERE username = '%s' AND repo_id = '%d'",
            $uid, $username, $repository['repo_id']);

  // Everything's done, let the world know about it!
  module_invoke_all('versioncontrol_account',
    'insert', $uid, $username, $repository, $additional_data
  );
}

/**
 * Update a VCS user account in the database, and call the necessary
 * module hooks. The @p $uid and @p $repository parameters must stay the same
 * values as the one given on account creation, whereas @p $username and
 * @p $vcs_specific may change.
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
function versioncontrol_update_account($uid, $username, $repository, $additional_data = array()) {
  $old_username = versioncontrol_get_account_username_for_uid($uid, $repository, TRUE);
  $username_changed = ($username != $old_username);

  if ($username_changed) {
    db_query("UPDATE {versioncontrol_accounts}
              SET username = '%s'
              WHERE uid = '%d' AND repo_id = '%d'",
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
              SET uid = '0'
              WHERE uid = '%d' AND repo_id = '%d'",
              $uid, $repository['repo_id']);
    db_query("UPDATE {versioncontrol_commits}
              SET uid = '%d'
              WHERE username = '%s' AND repo_id = '%d'",
              $uid, $username, $repository['repo_id']);
  }

  // Everything's done, let the world know about it!
  module_invoke_all('versioncontrol_account',
    'update', $uid, $username, $repository, $additional_data
  );
}

/**
 * Delete a VCS user account from the database, set all commits with this
 * account as author to user 0 (anonymous), and call the necessary hooks.
 *
 * @param $uid
 *   The Drupal user id corresponding to the VCS username.
 * @param $username
 *   The VCS specific username (a string).
 * @param $repository
 *   The repository where the user has its VCS account.
 */
function versioncontrol_delete_account($uid, $username, $repository) {
  // Update the commits table.
  db_query("UPDATE {versioncontrol_commits}
            SET uid = '0'
            WHERE uid = '%d' AND repo_id = '%d'",
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

  db_query("DELETE FROM {versioncontrol_accounts}
            WHERE uid = '%d' AND repo_id = '%d'",
            $uid, $repository['repo_id']);
}

/**
 * Insert a repository into the database, and call the necessary hooks.
 *
 * @param $repository
 *   The repository array containing the new or existing repository.
 *   It's a single repository array like the one returned by
 *   versioncontrol_get_repository(), so it consists of the following elements:
 *
 *   'repo_id': The unique repository id.
 *   'name': The user-visible name of the repository.
 *   'vcs': The unique string identifier of the version control system
 *          that powers this repository.
 *   'root': The root directory of the repository. In most cases,
 *           this will be a local directory (e.g. '/var/repos/drupal'),
 *           but it may also be some specialized string for remote repository
 *           access. How this string may look like depends on the backend.
 *   'url_backend': The prefix (excluding the trailing underscore)
 *                  for URL backend retrieval functions.
 *   '[xxx]_specific': An array of VCS specific additional repository
 *                     information. How this array looks like is defined by the
 *                     corresponding backend module (versioncontrol_[xxx]).
 *                     If the backend has registered itself with the
 *                     VERSIONCONTROL_FLAG_AUTOADD_REPOSITORIES option,
 *                     all items of this array will automatically be inserted
 *                     into the {versioncontrol_[xxx]_commits} table.
 *
 * @param $repository_urls
 *   An array of repository viewer URLs. How this array looks like is
 *   defined by the corresponding URL backend.
 */
function versioncontrol_insert_repository($repository, $repository_urls) {
  $repository['repo_id'] = db_next_id('{versioncontrol_repositories}_repo_id');

  db_query(
    "INSERT INTO {versioncontrol_repositories}
     (repo_id, name, vcs, root, url_backend)
     VALUES ('%d', '%s', '%s', '%s', '%s')",
     $repository['repo_id'], $repository['name'], $repository['vcs'],
     $repository['root'], $repository['url_backend']
  );
  // TODO: abstract out repository URLs into separate backends
  db_query(
    "INSERT INTO {versioncontrol_repository_urls}
     (repo_id, commit_view, file_view, directory_view, diff, tracker)
     VALUES ('%d', '%s', '%s', '%s', '%s', '%s')",
     $repository['repo_id'], $repository_urls['commit_view'],
     $repository_urls['file_view'], $repository_urls['directory_view'],
     $repository_urls['diff'], $repository_urls['tracker']
  );

  // Auto-add repository info from $repository['[xxx]_specific'] into the database.
  $backends = versioncontrol_get_backends();
  $is_autoadd = in_array(VERSIONCONTROL_FLAG_AUTOADD_REPOSITORIES,
                         $backends[$repository['vcs']]['flags']);
  if ($is_autoadd) {
    $table_name = 'versioncontrol_'. $repository['vcs'] .'_repositories';
    $elements = $repository[$repository['vcs'] .'_specific'];
    $elements['repo_id'] = $repository['repo_id'];
    _versioncontrol_db_insert_additions($table_name, $elements);
  }

  // Provide an opportunity for the backend to add its own stuff.
  if (versioncontrol_backend_implements($repository['vcs'], 'repository')) {
    _versioncontrol_call_backend(
      $repository['vcs'], 'repository', array('insert', $repository)
    );
  }

  // Everything's done, let the world know about it!
  module_invoke_all('versioncontrol_repository', 'insert', $repository);
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
 *   'repo_id': The unique repository id.
 *   'name': The user-visible name of the repository.
 *   'vcs': The unique string identifier of the version control system
 *          that powers this repository.
 *   'root': The root directory of the repository. In most cases,
 *           this will be a local directory (e.g. '/var/repos/drupal'),
 *           but it may also be some specialized string for remote repository
 *           access. How this string may look like depends on the backend.
 *   'url_backend': The prefix (excluding the trailing underscore)
 *                  for URL backend retrieval functions.
 *   '[xxx]_specific': An array of VCS specific additional repository
 *                     information. How this array looks like is defined by the
 *                     corresponding backend module (versioncontrol_[xxx]).
 *                     If the backend has registered itself with the
 *                     VERSIONCONTROL_FLAG_AUTOADD_REPOSITORIES option,
 *                     all items of this array will automatically be inserted
 *                     into the {versioncontrol_[xxx]_commits} table.
 *
 * @param $repository_urls
 *   An array of repository viewer URLs. How this array looks like is
 *   defined by the corresponding URL backend.
 */
function versioncontrol_update_repository($repository, $repository_urls) {
  db_query(
    "UPDATE {versioncontrol_repositories}
     SET name = '%s', vcs = '%s', root = '%s', url_backend = '%s'
     WHERE repo_id = '%d'",
     $repository['name'], $repository['vcs'], $repository['root'],
     $repository['url_backend'], $repository['repo_id']
  );
  // TODO: abstract out repository URLs into separate backends
  db_query(
    "UPDATE {versioncontrol_repository_urls}
     SET commit_view = '%s', file_view = '%s', directory_view = '%s',
         diff = '%s', tracker = '%s'
     WHERE repo_id = '%d'",
     $repository_urls['commit_view'], $repository_urls['file_view'],
     $repository_urls['directory_view'], $repository_urls['diff'],
     $repository_urls['tracker'], $repository['repo_id']
  );

  // Auto-add commit info from $commit['[xxx]_specific'] into the database.
  $backends = versioncontrol_get_backends();
  $is_autoadd = in_array(VERSIONCONTROL_FLAG_AUTOADD_REPOSITORIES,
                         $backends[$repository['vcs']]['flags']);
  if ($is_autoadd) {
    $table_name = 'versioncontrol_'. $repository['vcs'] .'_repositories';
    $elements = $repository[$repository['vcs'] .'_specific'];
    $elements['repo_id'] = $repository['repo_id'];
    _versioncontrol_db_update_additions($table_name, 'repo_id', $elements);
  }

  // Provide an opportunity for the backend to add its own stuff.
  if (versioncontrol_backend_implements($repository['vcs'], 'repository')) {
    _versioncontrol_call_backend(
      $repository['vcs'], 'repository', array('update', $repository)
    );
  }

  // Everything's done, let the world know about it!
  module_invoke_all('versioncontrol_repository', 'update', $repository);
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
  $commits = versioncontrol_get_commits(array('repo_ids' => array($repository['repo_id'])));
  foreach ($commits as $commit) {
    versioncontrol_delete_commit($commit);
  }

  $accounts = versioncontrol_get_accounts(
    array('repo_ids' => array($repository['repo_id'])), TRUE
  );
  foreach ($accounts as $uid => $usernames_by_repository) {
    foreach ($usernames_by_repository as $repo_id => $username) {
      versioncontrol_delete_account($uid, $username, $repository);
    }
  }

  // Announce deletion of the repository before anything has happened.
  module_invoke_all('versioncontrol_repository', 'delete', $repository);

  // Provide an opportunity for the backend to delete its own stuff.
  if (versioncontrol_backend_implements($repository['vcs'], 'repository')) {
    _versioncontrol_call_backend(
      $repository['vcs'], 'repository', array('delete', $repository)
    );
  }

  // Auto-delete repository info from $repository['[xxx]_specific'] from the database.
  $backends = versioncontrol_get_backends();
  $is_autoadd = in_array(VERSIONCONTROL_FLAG_AUTOADD_REPOSITORIES,
                         $backends[$repository['vcs']]['flags']);
  if ($is_autoadd) {
    $table_name = 'versioncontrol_'. $repository['vcs'] .'_repositories';
    _versioncontrol_db_delete_additions($table_name, 'repo_id', $repository['repo_id']);
  }

  db_query("DELETE FROM {versioncontrol_repositories} WHERE repo_id = '%d'",
           $repository['repo_id']);
  // TODO: abstract out repository URLs into separate backends
  db_query("DELETE FROM {versioncontrol_repository_urls} WHERE repo_id = '%d'",
           $repository['repo_id']);
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
    $types[] = is_numeric($value) ? "'%d'" : "'%s'";
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
    $type = is_numeric($value) ? "'%d'" : "'%s'";
    $set_statements[] = $key .' = '. $type;
    $params[] = is_numeric($value) ? $value : serialize($value);
  }
  $params[] = $elements[$primary_key_name];

  db_query(
    'UPDATE {'. $table_name .'}
     SET '. implode(', ', $set_statements) .'
     WHERE '. $primary_key_name ." = '%d'", $params
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
            WHERE '. $primary_key_name ." = '%d'", $primary_key);
}
