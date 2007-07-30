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
  // If not already there, retrieve the full repository object.
  if (!isset($commit['repository'])) {
    $repos = versioncontrol_get_repositories(array(
      'repo_ids' => array($commit['repo_id'])
    ));
    unset($commit['repo_id']);

    if (empty($repos)) {
      return NULL;
    }
    foreach ($repos as $repo_id => $repository) {
      $commit['repository'] = $repository; // there's only one array element
    }
  }

  // If not already there, retrieve the Drupal user id of the committer.
  if (!isset($commit['uid'])) {
    $authors = versioncontrol_get_authors_by_username(
      array($commit['repository']['repo_id'] => array($commit['username'])),
      array($commit['repository']['repo_id'] => $commit['repository'])
    );

    if (empty($authors)) {
      // If no uid could be retrieved, blame the commit on user 0 (anonymous).
      $commit['uid'] = 0;
    }
    else {
      foreach ($authors as $uid => $mapping) {
        $commit['uid'] = $uid;
        break; // we only asked for one username in one repository, that's it
      }
    }
  }

  // Also, retrieve the deepest-level directory for all commit actions.
  $commit['directory'] = _versioncontrol_get_common_directory($commit_actions);

  // Ok, everything's there, insert the commit into the database.
  $commit['commit_id'] = db_next_id('{versioncontrol_commits}_commit_id');

  db_query(
    "INSERT INTO {versioncontrol_commits}
     (commit_id, repo_id, date, uid, username, directory, message, revision)
     VALUES ('%d', '%d', '%d', '%d', '%s', '%s', '%s', '%s')",
    $commit['commit_id'], $commit['repository']['repo_id'],
    $commit['date'], $uid, $commit['username'], $commit['directory'],
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
  if (versioncontrol_backend_implements($commit['repository']['vcs'], 'insert_commit')) {
    _versioncontrol_call_backend(
      $commit['repository']['vcs'], 'insert_commit',
      array($commit, $commit_actions)
    );
  }

  // Everything's done, let the world know about it!
  module_invoke_all(
    'versioncontrol_commit', array('insert', $commit, $commit_actions)
  );

  return $commit;
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
 * @param $vcs_specific
 *   An array of VCS specific additional author information.
 *   How this array looks like is defined by the corresponding
 *   backend module (versioncontrol_[xxx]).
 *
 * @return
 *   A structured array with the combined account information that has been
 *   passed. Subsequently, it consists of the elements 'repo_id', 'username',
 *   and 'uid'.
 */
function versioncontrol_insert_account($uid, $username, $repository, $vcs_specific = NULL) {
  db_query(
    "INSERT INTO {versioncontrol_accounts} (uid, repo_id, username)
     VALUES ('%d', '%d', '%s')", $uid, $repository['repo_id'], $username
  );

  // Provide an opportunity for the backend to add its own stuff.
  if (versioncontrol_backend_implements($repository['vcs'], 'insert_account')) {
    _versioncontrol_call_backend(
      $repository['vcs'], 'insert_account',
      array($repository, $username, $uid, $vcs_specific)
    );
  }

  $account = array(
    'repo_id'  => $repository['repo_id'],
    'username' => $username,
    'uid'      => $uid,
  );

  // Everything's done, let the world know about it!
  module_invoke_all(
    'versioncontrol_account', array('insert', $account)
  );

  return $account;
}
