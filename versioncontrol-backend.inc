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
 * Copyright 2007, 2008 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */

/**
 * Determine if a commit, branch or tag operation may be executed or not.
 * Call this function inside a pre-commit hook.
 *
 * @param $operation
 *   A single operation array like the ones returned by
 *   versioncontrol_get_operations(), but leaving out on a few details that
 *   will instead be determined by this function. This array describes
 *   the operation that is about to happen. Here's the allowed elements:
 *
 *   - 'type': The type of the operation - one of the
 *        VERSIONCONTROL_OPERATION_{COMMIT,BRANCH,TAG} constants.
 *   - 'repository': The repository where this operation occurs,
 *        given as a structured array, like the return value
 *        of versioncontrol_get_repository().
 *        You can either pass this or 'repo_id'.
 *   - 'repo_id': The repository where this operation occurs, given as a simple
 *        integer id. You can either pass this or 'repository'.
 *   - 'uid': The Drupal user id of the committer. Passing this is optional -
 *        if it isn't set, this function will determine the uid.
 *   - 'username': The system specific VCS username of the committer.
 *   - 'message': The log message for the commit, tag or branch operation.
 *        If a version control system doesn't support messages for the current
 *        operation type, this element must not be set. Operations with
 *        log messages that are set but empty will be denied access.
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
 *        - 'type': Whether this label is a branch (indicated by the
 *             VERSIONCONTROL_OPERATION_BRANCH constant) or a tag
 *             (VERSIONCONTROL_OPERATION_TAG).
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
 *   elements for 'type', 'path', 'revision', 'action', 'source_items' and
 *   'replaced_item' - but doesn't include the 'item_revision_id' element as
 *   there's no relation to the database yet.
 *
 *   The 'action', 'source_items', 'replaced_item' and 'revision' elements
 *   of each item are optional and may be left unset.
 *
 * @return
 *   TRUE if the operation may happen, or FALSE if not.
 *   If FALSE is returned, you can retrieve the concerning error messages
 *   by calling versioncontrol_get_access_errors().
 */
function versioncontrol_has_write_access($operation, $operation_items) {
  $operation = _versioncontrol_fill_operation($operation);

  // If we can't determine this operation's repository,
  // we can't really allow the operation in the first place.
  if (!isset($operation['repository'])) {
    switch ($operation['type']) {
      case VERSIONCONTROL_OPERATION_COMMIT:
        $type = t('commit');
        break;
      case VERSIONCONTROL_OPERATION_BRANCH:
        $type = t('branch');
        break;
      case VERSIONCONTROL_OPERATION_TAG:
        $type = t('tag');
        break;
    }
    _versioncontrol_access_errors(array(t(
'** ERROR: Version Control API cannot determine a repository
** for the !commit-branch-or-tag information given by the VCS backend.',
      array('!commit-branch-or-tag' => $type)
    )));
    return FALSE;
  }

  // If the user doesn't have commit access at all, we can't allow this as well.
  $repo_data = $operation['repository']['data']['versioncontrol'];

  if (!$repo_data['allow_unauthorized_access']) {
    if (!versioncontrol_is_account_authorized($operation['repository'], $operation['uid'])) {
      _versioncontrol_access_errors(array(t(
        '** ERROR: !user does not have commit access to this repository.',
        array('!user' => $operation['username'])
      )));
      return FALSE;
    }
  }

  // Don't let people do empty log messages, that's as evil as it gets.
  if (isset($operation['message']) && empty($operation['message'])) {
    _versioncontrol_access_errors(array(
      t('** ERROR: You have to provide a log message.'),
    ));
    return FALSE;
  }

  // Also see if other modules have any objections.
  $error_messages = array();

  foreach (module_implements('versioncontrol_write_access') as $module) {
    $function = $module .'_versioncontrol_write_access';

    // If at least one hook_versioncontrol_write_access returns TRUE,
    // the commit goes through. (This is for admin or sandbox exceptions.)
    $outcome = $function($operation, $operation_items);
    if ($outcome === TRUE) {
      return TRUE;
    }
    else { // if !TRUE, $outcome is required to be an array with error messages
      $error_messages = array_merge($error_messages, $outcome);
    }
  }

  // Let the operation fail if there's more than zero error messages.
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

/**
 * Insert a commit, branch or tag operation into the database, and call the
 * necessary module hooks. Only call this function after the operation has been
 * successfully executed.
 *
 * @param $operation
 *   A single operation array like the ones returned by
 *   versioncontrol_get_operations(), but leaving out on a few details that
 *   will instead be determined by this function. Here's the allowed elements:
 *
 *   - 'type': The type of the operation - one of the
 *        VERSIONCONTROL_OPERATION_{COMMIT,BRANCH,TAG} constants.
 *   - 'repository': The repository where this operation occurs,
 *        given as a structured array, like the return value
 *        of versioncontrol_get_repository().
 *        You can either pass this or 'repo_id'.
 *   - 'repo_id': The repository where this operation occurs, given as a simple
 *        integer id. You can either pass this or 'repository'.
 *   - 'date': The time when the operation was performed, given as
 *        Unix timestamp. (For commits, this is the time when the revision
 *        was committed, whereas for branch/tag operations it is the time
 *        when the files were branched or tagged.)
 *   - 'uid': The Drupal user id of the committer. Passing this is optional -
 *        if it isn't set, this function will determine the uid.
 *   - 'username': The system specific VCS username of the user who executed
 *        this operation. For distributed version control systems, this should
 *        be the author, not the committer.
 *   - 'message': The log message for the commit, tag or branch operation.
 *        If a version control system doesn't support messages for the current
 *        operation type, this element should be empty.
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
 *        - 'name': The branch or tag name (a string).
 *        - 'type': Whether this label is a branch (indicated by the
 *             VERSIONCONTROL_OPERATION_BRANCH constant) or a tag
 *             (VERSIONCONTROL_OPERATION_TAG).
 *        - 'action': Specifies what happened to this label in this operation.
 *             For plain commits, this is always VERSIONCONTROL_ACTION_MODIFIED.
 *             For branch or tag operations (or commits that emulate those),
 *             it can be either VERSIONCONTROL_ACTION_ADDED or
 *             VERSIONCONTROL_ACTION_DELETED.
 *
 * @param $operation_items
 *   A structured array containing the exact details of happened to each
 *   item in this operation. The structure of this array is the same as
 *   the return value of versioncontrol_get_operation_items() - that is,
 *   elements for 'type', 'path' and 'revision' - but doesn't include the
 *   'item_revision_id' element, that one will be filled in by this function.
 *
 *   For commit operations, you also have to fill in the 'action' and
 *   'source_items' elements (and optionally 'replaced_item') that are also
 *   described in the versioncontrol_get_operation_items() API documentation.
 *   The 'line_changes' element, as in versioncontrol_get_operation_items(),
 *   is optional to provide.
 *
 *   This parameter is passed by reference as the insert operation will
 *   check the validity of a few item properties and will also assign an
 *   'item_revision_id' property to each of the given items. So when this
 *   function returns with a result other than NULL, the @p $operation_items
 *   array will also be up to snuff for further processing.
 *
 * @return
 *   The finalized operation array, with all of the 'vc_op_id', 'repository'
 *   and 'uid' properties filled in, and 'repo_id' removed if it existed before.
 *   Labels are now equipped with an additional 'label_id' property.
 *   (For more info on these labels, see the API documentation for
 *   versioncontrol_get_operations() and versioncontrol_get_operation_items().)
 *   In case of an error, NULL is returned instead of the operation array.
 */
function versioncontrol_insert_operation($operation, &$operation_items) {
  $operation = _versioncontrol_fill_operation($operation, TRUE);

  if (!isset($operation['repository'])) {
    return NULL;
  }

  // Ok, everything's there, insert the operation into the database.
  $operation['repo_id'] = $operation['repository']['repo_id']; // for drupal_write_record()
  drupal_write_record('versioncontrol_operations', $operation);
  unset($operation['repo_id']);
  // drupal_write_record() has now added the 'vc_op_id' to the $operation array.

  // Insert labels that are attached to the operation.
  $operation['labels'] = _versioncontrol_set_operation_labels($operation, $operation['labels']);

  $vcs = $operation['repository']['vcs'];

  // So much for the operation itself, now the more verbose part: items.
  ksort($operation_items); // similar paths should be next to each other

  foreach ($operation_items as $path => $item) {
    $item = _versioncontrol_sanitize_item($operation['repository'], $item);
    $item = _versioncontrol_ensure_item_revision(
      $operation['repository'], $item
    );
    _versioncontrol_insert_operation_item(
      $operation, $item, VERSIONCONTROL_OPERATION_MEMBER_ITEM
    );
    $item['selected_label'] = new stdClass();
    $item['selected_label']->get_from = 'operation';
    $item['selected_label']->successor_item = &$operation;

    // If we've got source items (which is the case for commit operations),
    // add them to the item revisions and source revisions tables as well.
    foreach ($item['source_items'] as $key => $source_item) {
      $source_item = _versioncontrol_ensure_item_revision(
        $operation['repository'], $source_item
      );
      _versioncontrol_insert_source_revision($item, $source_item, $item['action']);

      // Cache other important items in the operations table for 'path' search
      // queries, because joining the source revisions table is too expensive.
      switch ($item['action']) {
        case VERSIONCONTROL_ACTION_MOVED:
        case VERSIONCONTROL_ACTION_COPIED:
        case VERSIONCONTROL_ACTION_MERGED:
          if ($item['path'] != $source_item['path']) {
            _versioncontrol_insert_operation_item($operation,
              $source_item, VERSIONCONTROL_OPERATION_CACHED_AFFECTED_ITEM);
          }
          break;
        default: // No additional caching for added, modified or deleted items.
          break;
      }

      $source_item['selected_label'] = new stdClass();
      $source_item['selected_label']->get_from = 'other_item';
      $source_item['selected_label']->other_item = &$item;
      $source_item['selected_label']->other_item_tags = array('successor_item');

      $item['source_items'][$key] = $source_item;
    }
    // Plus a special case for the "added" action, as it needs an entry in the
    // source items table but contains no items in the 'source_items' property.
    if ($item['action'] == VERSIONCONTROL_ACTION_ADDED) {
      _versioncontrol_insert_source_revision($item, 0, $item['action']);
    }

    // If we've got a replaced item (might happen for copy/move commits),
    // add it to the item revisions and source revisions table as well.
    if (isset($item['replaced_item'])) {
      $item['replaced_item'] = _versioncontrol_ensure_item_revision(
        $operation['repository'], $item['replaced_item']
      );
      _versioncontrol_insert_source_revision(
        $item, $item['replaced_item'], VERSIONCONTROL_ACTION_REPLACED
      );
      $item['replaced_item']['selected_label'] = new stdClass();
      $item['replaced_item']['selected_label']->get_from = 'other_item';
      $item['replaced_item']['selected_label']->other_item = &$item;
      $item['replaced_item']['selected_label']->other_item_tags = array('successor_item');
    }
    $operation_items[$path] = $item;
  }

  // Notify the backend first.
  if (versioncontrol_backend_implements($vcs, 'operation')) {
    _versioncontrol_call_backend($vcs, 'operation', array(
      'insert', $operation, $operation_items
    ));
  }
  // Everything's done, let the world know about it!
  module_invoke_all('versioncontrol_operation',
    'insert', $operation, $operation_items
  );

  // This one too, as there is also an update function & hook for it.
  // Pretend that the labels didn't exist beforehand.
  $labels = $operation['labels'];
  $operation['labels'] = array();
  module_invoke_all('versioncontrol_operation_labels',
    'insert', $operation, $labels
  );
  $operation['labels'] = $labels;

  // Rules integration, because we like to enable people to be flexible.
  if (module_exists('rules')) {
    rules_invoke_event('versioncontrol_operation_insert', array(
      'operation' => $operation,
      'items' => $operation_items,
    ));
  }

  return $operation;
}

/**
 * Write @p $labels to the database as set of affected labels of the
 * given @p $operation. Label ids are not required to exist yet.
 *
 * @return
 *   The set of labels, all of them with 'label_id' filled in.
 */
function _versioncontrol_set_operation_labels($operation, $labels) {
  db_query("DELETE FROM {versioncontrol_operation_labels}
            WHERE vc_op_id = %d", $operation['vc_op_id']);

  foreach ($labels as &$label) {
    $label = versioncontrol_ensure_label($operation['repository'], $label);
    db_query("INSERT INTO {versioncontrol_operation_labels}
              (vc_op_id, label_id, action) VALUES (%d, %d, %d)",
              $operation['vc_op_id'], $label['label_id'], $label['action']);
  }
  return $labels;
}

/**
 * Replace the set of affected labels of the given @p $operation with the one
 * in @p $labels. If any of the given labels does not yet exist in the
 * database, a database entry (including new 'label_id' array element) will
 * be written as well.
 *
 * @return
 *   The updated operation, containing the new set of labels
 *   in $operation['labels'].
 */
function versioncontrol_update_operation_labels($operation, $labels) {
  module_invoke_all('versioncontrol_operation_labels',
    'update', $operation, $labels
  );
  $operation['labels'] = _versioncontrol_set_operation_labels($operation, $labels);
  return $operation;
}

/**
 * Check and if necessary correct item arrays so that item type and
 * the number of source items correspond to specified actions.
 */
function _versioncontrol_sanitize_item($repository, $item) {
  if (isset($item['action'])) {
    // Make sure the number of source items corresponds with the action.
    switch ($item['action']) {
      // No source items for "added" actions.
      case VERSIONCONTROL_ACTION_ADDED:
        if (count($item['source_items']) > 0) {
          _versioncontrol_bad_item_warning($repository, $item, 'At least one source item exists although the "added" action was set (which mandates an empty \'source_items\' array.');
          $item['source_items'] = array(reset($item['source_items'])); // first item
          $item['source_items'] = array();
        }
        break;
      // Exactly one source item for actions other than "added", "merged" or "other".
      case VERSIONCONTROL_ACTION_MODIFIED:
      case VERSIONCONTROL_ACTION_MOVED:
      case VERSIONCONTROL_ACTION_COPIED:
      case VERSIONCONTROL_ACTION_DELETED:
        if (count($item['source_items']) > 1) {
          _versioncontrol_bad_item_warning($repository, $item, 'More than one source item exists although a "modified", "moved", "copied" or "deleted" action was set (which allows only one of those).');
          $item['source_items'] = array(reset($item['source_items'])); // first item
        }
        // fall through
      case VERSIONCONTROL_ACTION_MERGED:
        if (empty($item['source_items'])) {
          _versioncontrol_bad_item_warning($repository, $item, 'No source item exists although a "modified", "moved", "copied", "merged" or "deleted" action was set (which requires at least or exactly one of those).');
        }
        break;
      default:
        break;
    }
    // For a "delete" action, make sure the item type is also a "deleted" one.
    // That's quite a minor error, so don't complain but rather fix it quietly.
    if ($item['action'] == VERSIONCONTROL_ACTION_DELETED) {
      if ($item['type'] == VERSIONCONTROL_ITEM_FILE) {
        $item['type'] = VERSIONCONTROL_ITEM_FILE_DELETED;
      }
      else if ($item['type'] == VERSIONCONTROL_ITEM_DIRECTORY) {
        $item['type'] = VERSIONCONTROL_ITEM_DIRECTORY_DELETED;
      }
    }
  }
  return $item;
}

/**
 * Print out a "Bad item received from VCS backend" warning to watchdog.
 */
function _versioncontrol_bad_item_warning($repository, $item, $message) {
  watchdog('special', "<p>Bad item received from VCS backend: !message</p>
    <pre>Item array: !item\nRepository array: !repository</pre>", array(
      '!message' => $message,
      '!item' => print_r($item, TRUE),
      '!repository' => print_r($repository, TRUE),
    ), WATCHDOG_ERROR
  );
}

/**
 * Insert an item entry into the {versioncontrol_item_revisions} table,
 * or retrieve the same one that's already there.
 *
 * @return
 *   The @p $item variable, enhanced with the newly added property
 *   'item_revision_id' specifying the database identifier for that revision.
 *   If the 'type' property of the passed item is different from the one in
 *   the database, then the new value will be written to the database.
 */
function _versioncontrol_ensure_item_revision($repository, $item) {
  $result = db_query(
    "SELECT item_revision_id, type
     FROM {versioncontrol_item_revisions}
     WHERE repo_id = %d AND path = '%s' AND revision = '%s'",
    $repository['repo_id'], $item['path'], $item['revision']
  );
  while ($item_revision = db_fetch_object($result)) {
    // Replace / fill in properties that were not in the WHERE condition.
    $item['item_revision_id'] = $item_revision->item_revision_id;

    if ($item['type'] == $item_revision->type) {
      return $item; // no changes needed - otherwise, replace the existing item.
    }
  }
  // The item doesn't yet exist in the database, so create it.
  return _versioncontrol_insert_item_revision($repository, $item);
}

function _versioncontrol_insert_item_revision($repository, $item) {
  $item['repo_id'] = $repository['repo_id']; // for drupal_write_record() only

  if (isset($item['item_revision_id'])) {
    // The item already exists in the database, update the record.
    drupal_write_record('versioncontrol_item_revisions', $item, 'item_revision_id');
  }
  else {
    // The label does not yet exist, create it.
    // drupal_write_record() also adds the 'item_revision_id' to the $item array.
    drupal_write_record('versioncontrol_item_revisions', $item);
  }
  unset($item['repo_id']);
  return $item;
}

/**
 * Insert an item entry into the {versioncontrol_operation_items} table.
 * The item is expected to have an 'item_revision_id' property already.
 */
function _versioncontrol_insert_operation_item($operation, $item, $type) {
  // Before inserting that item entry, make sure it doesn't exist already.
  db_query("DELETE FROM {versioncontrol_operation_items}
            WHERE vc_op_id = %d AND item_revision_id = %d",
            $operation['vc_op_id'], $item['item_revision_id']);

  db_query("INSERT INTO {versioncontrol_operation_items}
            (vc_op_id, item_revision_id, type) VALUES (%d, %d, %d)",
            $operation['vc_op_id'], $item['item_revision_id'], $type);
}

/**
 * Insert an item entry into the {versioncontrol_source_items} table.
 * Both target and source items are expected to have an 'item_revision_id'
 * property already. For "added" actions, it's also possible to pass 0 as the
 * @p $source_item parameter instead of a full item array.
 */
function _versioncontrol_insert_source_revision($item, $source_item, $action) {
  if ($action == VERSIONCONTROL_ACTION_ADDED && $source_item === 0) {
    $source_item = array('item_revision_id' => 0);
  }
  // Before inserting that item entry, make sure it doesn't exist already.
  db_query("DELETE FROM {versioncontrol_source_items}
            WHERE item_revision_id = %d AND source_item_revision_id = %d",
            $item['item_revision_id'], $source_item['item_revision_id']);

  $line_changes = !empty($item['line_changes']);
  db_query("INSERT INTO {versioncontrol_source_items}
            (item_revision_id, source_item_revision_id, action,
             line_changes_recorded, line_changes_added, line_changes_removed)
            VALUES (%d, %d, %d, %d, %d, %d)",
            $item['item_revision_id'], $source_item['item_revision_id'],
            $action, ($line_changes ? 1 : 0),
            ($line_changes ? $item['line_changes']['added'] : 0),
            ($line_changes ? $item['line_changes']['removed'] : 0));
}

/**
 * Delete a commit, a branch operation or a tag operation from the database,
 * and call the necessary hooks.
 *
 * @param $operation
 *   The commit, branch operation or tag operation array containing
 *   the operation that should be deleted.
 */
function versioncontrol_delete_operation($operation) {
  $operation_items = versioncontrol_get_operation_items($operation);

  // As versioncontrol_update_operation_labels() provides an update hook for
  // operation labels, we should also have a delete hook for completeness.
  module_invoke_all('versioncontrol_operation_labels',
                    'delete', $operation, array());
  // Announce deletion of the operation before anything has happened.
  // Calls hook_versioncontrol_commit(), hook_versioncontrol_branch_operation()
  // or hook_versioncontrol_tag_operation().
  module_invoke_all('versioncontrol_operation',
                    'delete', $operation, $operation_items);

  $vcs = $operation['repository']['vcs'];

  // Provide an opportunity for the backend to delete its own stuff.
  if (versioncontrol_backend_implements($vcs, 'operation')) {
    _versioncontrol_call_backend($vcs, 'operation', array(
      'delete', $operation, $operation_items
    ));
  }

  db_query('DELETE FROM {versioncontrol_operation_labels}
            WHERE vc_op_id = %d', $operation['vc_op_id']);
  db_query('DELETE FROM {versioncontrol_operation_items}
            WHERE vc_op_id = %d', $operation['vc_op_id']);
  db_query('DELETE FROM {versioncontrol_operations}
            WHERE vc_op_id = %d', $operation['vc_op_id']);
}

/**
 * Fill in various operation properties into the given operation array
 * (commit, branch op or tag op), in case those values are not given.
 *
 * @param $operation
 *   The plain operation array that might lack have some properties yet.
 * @param $include_unauthorized
 *   If FALSE, the 'uid' property will receive a value of 0 for known
 *   but unauthorized users. If TRUE, all known users are mapped to their uid.
 *
 * @return
 *   The completed commit, branch operation or tag operation array.
 *   Check on isset($operation['repository']) before proceeding.
 */
function _versioncontrol_fill_operation(&$operation, $include_unauthorized = FALSE) {
  // If not already there, retrieve the full repository object.
  if (!isset($operation['repository']) && isset($operation['repo_id'])) {
    $operation['repository'] = versioncontrol_get_repository($operation['repo_id']);
    unset($operation['repo_id']);
  }

  // If not already there, retrieve the Drupal user id of the committer.
  if (!isset($operation['uid'])) {
    $uid = versioncontrol_get_account_uid_for_username(
      $operation['repository']['repo_id'], $operation['username'], $include_unauthorized
    );
    // If no uid could be retrieved, blame the commit on user 0 (anonymous).
    $operation['uid'] = isset($uid) ? $uid : 0;
  }

  // For insertions (which have 'date' set, as opposed to write access checks),
  // fill in the log message if it's unset. We don't want to do this for
  // write access checks because empty messages are denied access,
  // which requires distinguishing between unset and empty.
  if (isset($operation['date']) && !isset($operation['message'])) {
    $operation['message'] = '';
  }
  return $operation;
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
 *   by implementing hook_versioncontrol_account_submit().
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

  // Update the operations table.
  db_query("UPDATE {versioncontrol_operations}
            SET uid = %d
            WHERE username = '%s' AND repo_id = %d",
            $uid, $username, $repository['repo_id']);

  // Everything's done, let the world know about it!
  module_invoke_all('versioncontrol_account',
    'insert', $uid, $username, $repository, $additional_data
  );

  watchdog('special',
    'Version Control API: added @username account in repository @repository',
    array('@username' => $username, '@repository' => $repository['name']),
    WATCHDOG_NOTICE, l('view', 'admin/project/versioncontrol-accounts')
  );
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
 *   by implementing hook_versioncontrol_account_submit().
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

  // Update the operations table.
  if ($username_changed) {
    db_query("UPDATE {versioncontrol_operations}
              SET uid = 0
              WHERE uid = %d AND repo_id = %d",
              $uid, $repository['repo_id']);
    db_query("UPDATE {versioncontrol_operations}
              SET uid = %d
              WHERE username = '%s' AND repo_id = %d",
              $uid, $username, $repository['repo_id']);
  }

  // Everything's done, let the world know about it!
  module_invoke_all('versioncontrol_account',
    'update', $uid, $username, $repository, $additional_data
  );

  watchdog('special',
    'Version Control API: updated @username account in repository @repository',
    array('@username' => $username, '@repository' => $repository['name']),
    WATCHDOG_NOTICE, l('view', 'admin/project/versioncontrol-accounts')
  );
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
  // Update the operations table.
  db_query('UPDATE {versioncontrol_operations}
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
    'Version Control API: deleted @username account in repository @repository',
    array('@username' => $username, '@repository' => $repository['name']),
    WATCHDOG_NOTICE, l('view', 'admin/project/versioncontrol-accounts')
  );
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
  if (isset($repository['repo_id'])) {
    // This is a new repository, it's not supposed to have a repo_id yet.
    unset($repository['repo_id']);
  }
  drupal_write_record('versioncontrol_repositories', $repository);
  // drupal_write_record() has now added the 'repo_id' to the $repository array.

  $repository_urls['repo_id'] = $repository['repo_id']; // for drupal_write_record()
  drupal_write_record('versioncontrol_repository_urls', $repository_urls);
  unset($repository_urls['repo_id']);

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
    'Version Control API: added repository @repository',
    array('@repository' => $repository['name']),
    WATCHDOG_NOTICE, l('view', 'admin/project/versioncontrol-repositories')
  );
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
  drupal_write_record('versioncontrol_repositories', $repository, 'repo_id');

  $repository_urls['repo_id'] = $repository['repo_id']; // for drupal_write_record()
  drupal_write_record('versioncontrol_repository_urls', $repository_urls, 'repo_id');
  unset($repository_urls['repo_id']);

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
    'Version Control API: updated repository @repository',
    array('@repository' => $repository['name']),
    WATCHDOG_NOTICE, l('view', 'admin/project/versioncontrol-repositories')
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
  // Delete operations.
  $operations = versioncontrol_get_operations(array('repo_ids' => array($repository['repo_id'])));
  foreach ($operations as $operation) {
    versioncontrol_delete_operation($operation);
  }
  unset($operations); // conserve memory, this might get quite large

  // Delete labels.
  db_query('DELETE FROM {versioncontrol_labels}
            WHERE repo_id = %d', $repository['repo_id']);

  // Delete item revisions and related source item entries.
  $result = db_query('SELECT item_revision_id
                      FROM {versioncontrol_item_revisions}
                      WHERE repo_id = %d', $repository['repo_id']);
  $item_ids = array();
  $placeholders = array();

  while ($item_revision = db_fetch_object($result)) {
    $item_ids[] = $item_revision->item_revision_id;
    $placeholders[] = '%d';
  }
  if (!empty($item_ids)) {
    $placeholders = '('. implode(',', $placeholders) .')';

    db_query('DELETE FROM {versioncontrol_source_items}
              WHERE item_revision_id IN '. $placeholders, $item_ids);
    db_query('DELETE FROM {versioncontrol_source_items}
              WHERE source_item_revision_id IN '. $placeholders, $item_ids);
    db_query('DELETE FROM {versioncontrol_item_revisions}
              WHERE repo_id = %d', $repository['repo_id']);
  }
  unset($item_ids); // conserve memory, this might get quite large
  unset($placeholders); // ...likewise

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
  db_query('DELETE FROM {versioncontrol_repository_urls} WHERE repo_id = %d',
           $repository['repo_id']);
  db_query('DELETE FROM {versioncontrol_repository_metadata} WHERE repo_id = %d',
           $repository['repo_id']);

  watchdog('special',
    'Version Control API: deleted repository @repository',
    array('@repository' => $repository['name']),
    WATCHDOG_NOTICE, l('view', 'admin/project/versioncontrol-repositories')
  );
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

  if (empty($set_statements)) {
    return; // no use updating the database if no values are assigned.
  }

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
