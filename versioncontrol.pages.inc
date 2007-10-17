<?php
// $Id$
/**
 * @file
 * Version Control API - An interface to version control systems
 * whose functionality is provided by pluggable back-end modules.
 *
 * This file contains the user interface for non-admin tasks.
 *
 * Copyright 2006, 2007 Derek Wright ("dww" , http://drupal.org/user/46549)
 * Copyright 2007 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */


/**
 * Form callback for "versioncontrol/register":
 * Provide an indirection that leads to an account registration form.
 */
function versioncontrol_account_register_page() {
  global $user;
  $presets = _versioncontrol_get_string_presets();
  $form = array();

  if (arg(2) == 'demo') { // demo mode for showing the page to the admin
    $uid = -1;
  }
  else {
    $uid = is_numeric(arg(2)) ? arg(2) : $user->uid;
  }
  $admin_access = user_access('administer version control systems');
  $user_access = user_access('use version control systems') || $admin_access;

  if (!$user_access || $user->uid == 0) {
    return variable_get(
      'versioncontrol_registration_message_unauthorized',
      $presets['versioncontrol_registration_message_unauthorized']
    );
  }
  if (!$admin_access && $uid != $user->uid) {
    drupal_access_denied();
    return;
  }

  $accounts = versioncontrol_get_accounts(array('uids' => array($uid)), TRUE);
  $repositories = versioncontrol_get_repositories();

  // Construct the '#options' array.
  $account_repo_ids = array();
  foreach ($accounts as $uid => $usernames_by_repository) {
    foreach ($usernames_by_repository as $repo_id => $username) {
      $account_repo_ids[] = $repo_id;
    }
  }

  if (arg(2) == 'demo') { // demo mode for showing the page to the admin
    $account_repo_ids = array();
  }

  $repository_names = array();
  foreach ($repositories as $repo_id => $repository) {
    if (in_array($repo_id, $account_repo_ids)
        && versioncontrol_is_account_authorized($uid, $repository)) {
      // We only want repositories in the list of repositories that are open
      // for registrations where no (authorized) account exists yet.
      continue;
    }
    if (!isset($first_repo_id)) {
      $first_repo_id = $repo_id;
    }
    $repository_names[$repo_id] = check_plain($repository['name']);
  }

  // Filter (and possible change the caption of) the list of repositories to select.
  // The admin has all the permissions and gets the original selection.
  if (!$admin_access) {
    foreach (module_implements('versioncontrol_alter_repository_selection') as $module) {
      $function = $module .'_versioncontrol_alter_repository_selection';
      $function($repository_names, $repositories);
    }
  }

  // If there's just one repository on the site, redirect directly to this one.
  if (count($repository_names) == 1) {
    $only_repo_id = $first_repo_id;
  }
  else if (count($repositories) == 1) {
    foreach ($repositories as $repo_id => $repository) {
      $only_repo_id = $repo_id;
    }
  }

  if (arg(2) == 'demo') { // demo mode for showing the page to the admin
    unset($only_repo_id);
  }

  if (is_numeric(arg(3)) || isset($only_repo_id)) {
    $repo_id = isset($only_repo_id) ? $only_repo_id : arg(3);
    if (in_array($repo_id, $account_repo_ids)) {
      drupal_set_message(t('You already have a registered account.'));
      drupal_goto('user/'. $user->uid .'/edit/versioncontrol/'. $repo_id);
      return '';
    }
    else if (isset($repository_names[$repo_id])) {
      return drupal_get_form('versioncontrol_account_edit_form',
                             $uid, '', $repositories[$repo_id]);
    }
  }

  return drupal_get_form('versioncontrol_account_register_form',
                         $uid, $repository_names, $first_repo_id);
}

/**
 * Implementation of hook_versioncontrol_alter_repository_selection():
 * Alter the list of repositories that are available for user registration
 * and editing. This hook is called for all users except those with
 * "administer version control systems" permissions.
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
 */
function versioncontrol_versioncontrol_alter_repository_selection(&$repository_names, $repositories) {
  foreach ($repository_names as $repo_id => $caption) {
    if ($repositories[$repo_id]['authorization_method'] == 'versioncontrol_admin') {
      unset($repository_names[$repo_id]);
    }
  }
}

/**
 * The actual form for "versioncontrol/register".
 */
function versioncontrol_account_register_form($uid, $repository_names, $first_repo_id) {
  if (empty($repository_names)) {
    drupal_set_title(t('No more registrations possible'));
    $form['no_registration'] = array(
      '#type' => 'markup',
      '#value' => t('You already have an account for all the repositories known to this site. Go to your !user-account-page to configure repository account settings.',
        array('!user-account-page' => l(t('user account page'), 'user/'. $uid .'/edit'))
      ),
      '#prefix' => '<p>',
      '#suffix' => '</p>',
    );
    return $form;
  }

  $form['#id'] = 'vcs-account-indirection-form';

  $message = variable_get(
    'versioncontrol_registration_message_authorized',
    $presets['versioncontrol_registration_message_authorized']
  );
  if (!empty($message)) {
    $form['overview'] = array(
      '#type' => 'fieldset',
      '#title' => t('Overview'),
      '#weight' => -100,
    );
    $form['overview']['overview'] = array(
      '#type' => 'markup',
      '#value' => $message,
    );
  }

  $form['uid'] = array(
    '#type' => 'value',
    '#value' => $uid,
  );

  $form['repository'] = array(
    '#type' => 'fieldset',
    '#title' => t('Select repository'),
    '#weight' => 10,
  );
  $form['repository']['repo_id'] = array(
    '#type' => 'select',
    '#title' => t('Create user account in'),
    '#options' => $repository_names,
    '#default_value' => $first_repo_id,
  );

  if (arg(2) != 'demo') { // demo mode for showing the page to the admin
    $form['repository']['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Create account'),
      '#weight' => 100,
    );
  }
  return $form;
}

/**
 * Submit handler for the indirection form.
 * Surprisingly, all it does is redirect to the appropriate registration form.
 */
function versioncontrol_account_register_form_submit($form_id, $form_values) {
  return 'versioncontrol/register/'. $form_values['uid'] .'/'. $form_values['repo_id'];
}


/**
 * Retrieve the message that shows up on the repository edit form when a
 * new user registers an account.
 *
 * Only call this function from within a form_alter() that modifies the
 * account edit form.
 *
 * Note: I'm not really satisfied with this being a public function,
 * but the Account Status module needs to display this message also for
 * reapplications, which is not included in the original form.
 * But it seems to be a better solution than exposing the
 * {versioncontrol_repository_metadata} table or even adding the message
 * to the {versioncontrol_repository} table. For now, at least.
 *
 * @param $repo_id
 *   The repository id where the a new account is registered.
 *
 * @return
 *   The registration message if it has been set, or an empty string otherwise.
 */
function versioncontrol_get_repository_registration_message($repo_id) {
  $result = db_query("SELECT registration_message
                      FROM {versioncontrol_repository_metadata}
                      WHERE repo_id = '%d'", $repo_id);
  while ($row = db_fetch_object($result)) {
    return $row->registration_message;
  }
  return '';
}

/**
 * Form callback for "user/$uid/edit/versioncontrol/$repository[repo_id]",
 * "versioncontrol/register/$uid/$repository[repo_id]" and (in certain cases)
 * "versioncontrol/register/$uid" or just "versioncontrol/register":
 * Provide a form to edit or register a VCS user account.
 *
 * @param $uid
 *   The Drupal user id of the user whose account is being edited.
 * @param $username
 *   The VCS specific username of that user. If this is an empty string,
 *   it means that a user account for this repository does not yet exist
 *   and should be created.
 * @param $repository
 *   The repository of the added/edited account.
 */
function versioncontrol_account_edit_form($uid, $username, $repository) {
  $form = array();
  $is_new_account = empty($username);
  $backends = versioncontrol_get_backends();
  $vcs_name = $backends[$repository['vcs']]['name'];
  $admin_access = user_access('administer version control systems');

  $form['#id'] = 'versioncontrol-account-form';
  $form['#repository'] = $repository;
  $form['#vcs'] = $repository['vcs'];
  $form['#validate'] = array('versioncontrol_account_edit_form_validate' => array());
  $form['#submit'] = array('versioncontrol_account_edit_form_submit' => array());

  // Values stored for later processing.
  $form['uid'] = array(
    '#type' => 'value',
    '#value' => $uid,
  );
  $form['original_username'] = array(
    '#type' => 'value',
    '#value' => $username,
  );
  $form['repo_id'] = array(
    '#type' => 'value',
    '#value' => $repository['repo_id'],
  );
  $form['vcs_name'] = array(
    '#type' => 'value',
    '#value' => $vcs_name,
  );

  if ($is_new_account) {
    $registration_message = versioncontrol_get_repository_registration_message($repository['repo_id']);

    if (!empty($registration_message)) {
      $form['overview'] = array(
        '#type' => 'fieldset',
        '#title' => t('Overview'),
        '#weight' => -100,
      );
      $form['overview']['overview'] = array(
        '#type' => 'markup',
        '#value' => $registration_message,
      );
    }
  }

  $form['account'] = array(
    '#type' => 'fieldset',
    '#title' => t('@vcs account settings', array('@vcs' => $vcs_name)),
    '#weight' => 0,
  );

  if ($is_new_account || $admin_access) {
    $user = user_load(array('uid' => $uid));

    // Set a more appropriate title for account registrations.
    if ($is_new_account) {
      drupal_set_title(t(
        'Create user account in @repository',
        array('@repository' => $repository['name'])
      ));

      // Have a nice default value for the new VCS username.
      $username = strtr(strtolower($user->name),
        array(' ' => '', '@' => '', '.' => '', '-' => '', '_' => '', '.' => '')
      );
    }
    else { // For the tab on the user edit page:
      // When someone else than the user goes here (the admin, that is),
      // the page title is incorrect and would say 'User', which is bad.
      drupal_set_title(check_plain($user->name));
    }

    if ($admin_access) { // the admin version
      $description = t('The @vcs username associated with the account of !user. This field is used to link commit messages to user accounts.', array('@vcs' => $vcs_name, '!user' => theme_username($user)));
    }
    else { // the account creation version
      $description = t('Choose a username to access the @vcs repository with. @vcs usernames should be lowercase. Spaces or other special characters are not allowed.', array('@vcs' => $vcs_name));
    }

    $form['account']['account_name'] = array(
      '#type' => 'textfield',
      '#title' => t('@vcs username', array('@vcs' => $vcs_name)),
      '#description' => $description,
      '#default_value' => $username,
      '#weight' => 0,
      '#size' => 30,
      '#maxlength' => 64,
    );
  }
  else {
    $form['account_name'] = array(
      '#type' => 'value',
      '#value' => $username,
    );
    $form['account']['account_name_display'] = array(
      '#type' => 'item',
      '#title' => t('@vcs username', array('@vcs' => $vcs_name)),
      '#description' => t('Your @vcs username. This field can only be edited by administrators and is used to link your @vcs messages to your user account.', array('@vcs' => $vcs_name)),
      '#value' => $username,
      '#weight' => 0,
    );
  }

  if (arg(2) != 'demo') { // demo mode for showing the page to the admin
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => $is_new_account
                  ? t('Create @vcs account', array('@vcs' => $vcs_name))
                  : t('Update @vcs account', array('@vcs' => $vcs_name)),
      '#weight' => 100,
    );
  }
  return $form;
}

/**
 * Validate the edit/register user account form submission before it is submitted.
 */
function versioncontrol_account_edit_form_validate($form_id, $form_values) {
  if (!isset($form_values['account_name'])) {
    return;
  }
  $uid = $form_values['uid'];
  $username = trim($form_values['account_name']);
  $repository = versioncontrol_get_repository($form_values['repo_id']);
  $vcs_name = $form_values['vcs_name'];

  if (!isset($repository)) { // admin deletes repo while user fills in form
    form_set_error('account',
      t('The repository has been deleted.', array('@vcs' => $vcs_name))
    );
    return;
  }

  if (empty($username)) {
    form_set_error('account_name',
      t('The @vcs username may not be empty.', array('@vcs' => $vcs_name))
    );
  }
  else {
    // Check for unallowed characters. (Only alphanumeric ones are allowed.)
    if (ereg("[^[:alnum:]]", $username)) {
      form_set_error('account_name',
        t('The specified @vcs username is invalid.', array('@vcs' => $vcs_name))
      );
    }
    // Check for duplicates.
    $existing_uid = versioncontrol_get_account_uid_for_username($repository['repo_id'], $username, TRUE);
    if ($existing_uid && $uid != $existing_uid) {
      if ($existing_user = user_load(array('uid' => $existing_uid))) {
        $existing_username = theme('username', $existing_user);
      }
      else {
        $existing_username = t('user #!id', array('!id' => $existing_uid));
      }
      form_set_error('account_name',
        t('The specified @vcs username is already in use by !existing-user.',
          array('@vcs' => $vcs_name, '!existing-user' => $existing_username))
      );
    }
  }
}

/**
 * Add or update the user account when the edit/register form is submitted.
 */
function versioncontrol_account_edit_form_submit($form_id, $form_values) {
  // Reconstruct the user data from the $form_values that were passed.
  $uid = $form_values['uid'];
  $username = trim($form_values['account_name']);
  $repository = versioncontrol_get_repository($form_values['repo_id']);
  $vcs_name = $form_values['vcs_name'];
  $vcs_specific = NULL;

  $additional_data = module_invoke_all(
    'versioncontrol_extract_account_data', $form_values
  );

  if (empty($form_values['original_username'])) {
    versioncontrol_insert_account($repository, $uid, $username, $additional_data);
    $message = drupal_set_message(t(
      'The @vcs account %username has been registered.',
      array('@vcs' => $vcs_name, '%username' => $username)
    ));
  }
  else {
    versioncontrol_update_account($repository, $uid, $username, $additional_data);

    // Regular users cannot change the username, and will probably get
    // a message for each of the other actions that hook into the form.
    if (user_access('administer version control systems')) {
      $message = drupal_set_message(t(
        'The @vcs account %username has been updated successfully.',
        array('@vcs' => $vcs_name, '%username' => $username)
      ));
    }
  }
  if (user_access('administer version control systems')) {
    return 'admin/project/versioncontrol-accounts';
  }
  else {
    return 'user/'. $uid .'/edit/versioncontrol/'. $repository['repo_id'];
  }
}
