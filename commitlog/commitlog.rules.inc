<?php
// $Id$
/**
 * @file
 * Commit Log - Display a history of commits, branches and tags,
 * optionally filtered by a set of constraint arguments.
 *
 * This file provides support for Rules.
 *
 * Copyright 2009 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */

/**
 * Implementation of hook_rules_action_info().
 */
function commitlog_rules_action_info() {
  return array(
    'commitlog_action_send_notification_mail' => array(
      'label' => t('Send commit notification mail to an email address'),
      'arguments' => array(
        'mailto' => array(
          'type' => 'string',
          'label' => t('Send to mail address(es)'),
          'description' => t('You can also specify multiple receivers by separating their mail addresses with a comma.'),
        ),
        'operation' => array(
          'type' => 'versioncontrol_operation',
          'label' => t('Operation'),
          'description' => t('The version control operation (commit, or branch/tag creation/deletion) that will be used as contents of the notification mail.'),
        ),
        'operation_items' => array(
          'type' => 'versioncontrol_item_list',
          'label' => t('Operation items'),
        ),
      ),
      'module' => t('Commit Log'),
    ),
    'commitlog_action_send_user_notification_mail' => array(
      'label' => t('Send commit notification mail to a registered user'),
      'arguments' => array(
        'account' => array(
          'type' => 'user',
          'label' => t('Send to user'),
        ),
        'operation' => array(
          'type' => 'versioncontrol_operation',
          'label' => t('Operation'),
        ),
        'operation_items' => array(
          'type' => 'versioncontrol_item_list',
          'label' => t('Operation items'),
        ),
      ),
      'module' => t('Commit Log'),
    ),
  );
}

function commitlog_action_send_notification_mail($mailto, $operation, $operation_items) {
  // In order to keep privacy, each recipient only gets to see himself in
  // the "To:" header. So don't send the mail directly but just construct
  // the mail contents, and send it separately for each recipient afterwards.
  $send = FALSE;
  $message = commitlog_notification_mail('', $operation, $operation_items, $send);

  foreach (explode(',', $mailto) as $address) {
    $message['to'] = trim($address);
    $result = drupal_mail_send($message);

    // Log errors.
    if (!$result) {
      watchdog('mail', 'Error sending commit notification e-mail (from %from to %to).', array(
        '%from' => $message['from'],
        '%to' => $message['to'],
      ), WATCHDOG_ERROR);
    }
  }
}

function commitlog_action_send_user_notification_mail($account, $operation, $operation_items) {
  commitlog_user_notification_mail($account, $operation, $operation_items);
}
