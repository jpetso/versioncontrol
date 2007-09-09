<?php
// $Id$
/**
 * @file
 * Version Control Account Status - site specific overrides.
 *
 * Rename this file to versioncontrol_account_status_local.inc and
 * customize to your liking. None of the functions in here are required,
 * and default behaviour will be used instead if they are being left out.
 *
 * Copyright 2006, 2007 by Derek Wright ("dww", http://drupal.org/user/46549)
 * Copyright 2007 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */

/**
 * Provide a site specific list of necessary requirements that each applicant
 * must agree to in order to get the application approved.
 *
 * @return
 *   An array of conditions that are afterwards transformed into form elements.
 *   Each condition will be shown to the user together with "No"/"Yes"
 *   radio buttons. All the elements must be marked with "Yes" by the user
 *   in order to get the application submitted.
 *   The array key of each element will be the array key of the corresponding
 *   form element, and the value is the again a structured array that contains
 *   the following user-visible (translated) strings:
 *   - 'description': The statement that is shown on the application form.
 *   - 'error': The error message that is shown if the user stays with "No".
 */
function versioncontrol_account_status_local_conditions() {
  return array(
    'maintain' => array(
      'description' => t('I want to maintain a module, theme or translation'),
      'error' => t('Your application cannot be considered: CVS accounts are only granted to users wishing to maintain a module, theme or translation.'),
    ),
    'license' => array(
      'description' => t('I will only commit code that is licensed under terms of the <a href="http://www.gnu.org/licenses/old-licenses/gpl-2.0.html">GNU public license, version 2.0</a>'),
      'error' => t('Your application cannot be considered: all contributions in the CVS repository must be licensed under the terms of the GPL.'),
    ),
  );
}
