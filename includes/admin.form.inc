<?php

/**
 * @file
 * Administration forms and submit handling for Islandora CSV to MODS Utility module.
 */

/**
 * Administration and configuration form for islandora_csv_to_mods.
 *
 * @ingroup forms
 *
 * @todo break out non-settings elements into their own form.
 *
 * @return array
 *   The Drupal form definition.
 */
function islandora_csv_to_mods_admin_form() {
  module_load_include('inc', 'islandora_csv_to_mods', 'includes/utilities');
  $csv_to_mods_mappings = islandora_csv_to_mods_get_csv_header_xpath_mappings();
  $csv_to_mods_mapping_lines = array();
  foreach ($csv_to_mods_mappings as $field => $xpath) {
    $csv_to_mods_mapping_lines[] = '<div><div class="fl wide_200"><b>' . $field . '</b></div><div class="fl">' . $xpath . '</div></div>';
  }

  $form = array(
    'namespace' => array(
      '#type' => 'textfield',
      '#title' => t('Object Namespace'),
      '#description' => t('The namespace to use for CSV import to convert identifier into.  Do not include the colon character.'),
      '#default_value' => variable_get('islandora_csv_to_mods_namespace'),
    ),
    'submit' => array(
      '#type' => 'submit',
      '#value' => t('Save'),
    ),
    'frame' => array(
      '#type' => 'fieldset',
      '#title' => 'Islandora CSV to MODS Utility',
      'about' => array(
        '#type' => 'item',
        '#markup' => l('Import CSV', 'islandora/csv_to_mods/import_csv') . ' - upload CSV for MODS updates.',
      ),
      'csv_to_mods_mappings' => array(
        '#type' => 'item',
        '#markup' => '<h3>Supported CSV fields</h3>' . 
          '<div><div class="fl heading wide_200">CSV heading</div><div class="fl heading">MODS node (xpath)</div></div><br>' .
          implode("<br>", $csv_to_mods_mapping_lines),
      )
    ),
  );
  return $form;
}

/**
 * Function that sets the Drupal variables with user's input.
 *
 * @param array $form
 *   The Drupal form definition.
 * @param array $form_state
 *   The Drupal form state.
 */
function islandora_csv_to_mods_admin_form_submit(array $form, array &$form_state) {
  $id = $form_state['triggering_element']['#id'];
  switch ($id) {
    case 'edit-submit':
      variable_set('islandora_csv_to_mods_namespace', str_replace(":", "", $form_state['values']['namespace']));
      break;

    case 'edit-reset':
      break;
  }
}
