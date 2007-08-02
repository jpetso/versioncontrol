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
    $repository = versioncontrol_get_repository($commit['repo_id']);
    unset($commit['repo_id']);

    if (!isset($repository)) {
      return NULL;
    }
    $commit['repository'] = $repository;
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
  module_invoke_all(
    'versioncontrol_commit', array('delete', $commit, $commit_actions)
  );

  // Provide an opportunity for the backend to delete its own stuff.
  if (versioncontrol_backend_implements($repository['vcs'], 'delete_commit')) {
    _versioncontrol_call_backend(
      $commit['repository']['vcs'], 'delete_commit', array($commit, $commit_actions)
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

  // Update the commits table.
  db_query("UPDATE {versioncontrol_commits}
            SET uid = '%d'
            WHERE username = '%s' AND repo_id = '%d'",
            $uid, $username, $repository['repo_id']);

  // Everything's done, let the world know about it!
  module_invoke_all(
    'versioncontrol_account', array('insert', $uid, $username, $repository)
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
  module_invoke_all(
    'versioncontrol_account', array('delete', $uid, $username, $repository)
  );

  // Provide an opportunity for the backend to delete its own stuff.
  if (versioncontrol_backend_implements($repository['vcs'], 'delete_account')) {
    _versioncontrol_call_backend(
      $repository['vcs'], 'delete_account', array($uid, $username, $repository)
    );
  }

  db_query("DELETE FROM {versioncontrol_accounts}
            WHERE uid = '%d' AND repo_id = '%d'",
            $uid, $repository['repo_id']);
}

/**
 * Insert a repository into the database or update an existing one,
 * and call the necessary module hooks.
 *
 * @param $repository
 *   The repository array containing the new or existing repository.
 *   It's a single repository array like the one returned by
 *   versioncontrol_get_repository(), so it consists of the following elements:
 *
 *   'repo_id': The unique repository id. If this is 0 or not set,
 *              the repository is assumed not to exist yet, and will be
 *              inserted into the database. Otherwise the existing entry
 *              will be updated.
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
function versioncontrol_set_repository($repository, $repository_urls) {
  if (isset($repository['repo_id']) && $repository['repo_id'] > 0) {
    _versioncontrol_update_repository($repository, $repository_url);
  }
  else {
    $repository['repo_id'] = db_next_id('{versioncontrol_repositories}_repo_id');
    _versioncontrol_insert_repository($repository, $repository_url);
  }
}

/**
 * Update a repository in the database, and call the necessary hooks.
 */
function _versioncontrol_update_repository($repository, $repository_url) {
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
  if (versioncontrol_backend_implements($repository['vcs'], 'update_repository')) {
    _versioncontrol_call_backend(
      $repository['vcs'], 'update_repository', array($repository)
    );
  }

  // Everything's done, let the world know about it!
  module_invoke_all(
    'versioncontrol_repository', array('update', $repository)
  );
}

/**
 * Insert a repository into the database, and call the necessary hooks.
 */
function _versioncontrol_insert_repository($repository, $repository_url) {
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
  if (versioncontrol_backend_implements($repository['vcs'], 'insert_repository')) {
    _versioncontrol_call_backend(
      $repository['vcs'], 'insert_repository', array($repository)
    );
  }

  // Everything's done, let the world know about it!
  module_invoke_all(
    'versioncontrol_repository', array('insert', $repository)
  );
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

  $accounts = versioncontrol_get_accounts(array('repo_ids' => array($repository['repo_id'])));
  foreach ($accounts as $uid => $usernames_by_repository) {
    foreach ($usernames_by_repository as $repo_id => $username) {
      versioncontrol_delete_account($uid, $username, $repository);
    }
  }

  // Announce deletion of the repository before anything has happened.
  module_invoke_all(
    'versioncontrol_repository', array('delete', $repository)
  );

  // Provide an opportunity for the backend to delete its own stuff.
  if (versioncontrol_backend_implements($repository['vcs'], 'delete_repository')) {
    _versioncontrol_call_backend(
      $repository['vcs'], 'delete_repository', array($repository)
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
