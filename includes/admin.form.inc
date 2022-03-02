<?php

/**
 * @file
 * Administration forms and submit handling for Islandora CSV to MODS Utility.
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

  $mods_schema_versions = islandora_csv_to_mods_get_mods_schema_versions();

  $transforms_k_v = array();
  if (module_exists('xml_form_builder')) {
    $associations = array();
    module_load_include('inc', 'xml_form_builder', 'includes/associations');
    $transforms = xml_form_builder_get_transforms($associations);
    // Why does this seem to be using the value for the keys?
    foreach ($transforms as $k => $v) {
      $transforms_k_v[$v] = $k;
    }
    $mods_to_dc_desc = 'The xslt transform to convert MODS to DC.  Leave blank ' .
        'if any resultant MODS should not generate DC.';
  }
  else {
    $transforms['No Transform'] = 'No Transform';
    $mods_to_dc_desc = 'The islandora_xml_forms module is not installed.  ' .
        'Install it to be able to use the "mods_to_dc.xsl" transforms that ' .
        'are configured there.';
  }

  $form = array(
    'namespace' => array(
      '#type' => 'textfield',
      '#title' => t('Object Namespace'),
      '#description' => t('The namespace to use for CSV import to convert identifier into.  Do not include the colon character.  If the `identifier` column contains a full PID value, the namespace specified by that value would override this namespace setting.'),
      '#default_value' => variable_get('islandora_csv_to_mods_namespace'),
    ),
    'mods_to_dc_transform' => array(
      '#type' => 'select',
      '#options' => $transforms_k_v,
      '#default_value' => variable_get('islandora_csv_to_mods_mods_to_dc_transform', 'No Transform'),
      '#title' => 'MODS to DC XSL Transform',
      '#description' => $mods_to_dc_desc,
    ),
    'schema_frame' => array(
      '#type' => 'fieldset',
      '#title' => 'Schema checking',
      'schema_test_text_xml' => array(
        '#type' => 'checkbox',
        '#title' => 'Perform schema test on import or transform to object MODS.',
        '#default_value' => variable_get('islandora_csv_to_mods_schema_test_text_xml', TRUE),
      ),
      'mods_schema_xsd_version' => array(
        '#type' => 'select',
        '#title' => 'MODS Schema',
        '#description' => 'Select the version of the MODS schema to be used for ' .
          'checking incoming MODS xml.',
        '#options' => $mods_schema_versions,
        '#default_value' => variable_get('islandora_csv_to_mods_mods_schema_xsd_version', 'MODS 3.0'),
      ),
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
        '#markup' => l(t('Import CSV'), 'islandora/csv_to_mods/import_csv') . ' - upload CSV for MODS updates.',
      ),
      'csv_to_mods_mappings' => array(
        '#type' => 'item',
        '#markup' => '<h3>Supported CSV fields</h3>' .
        '<div><div class="fl heading wide_200">CSV heading</div><div class="fl heading">MODS node (xpath)</div></div><br class="fl_break">' .
        implode("<br>", $csv_to_mods_mapping_lines),
      ),
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
      variable_set('islandora_csv_to_mods_mods_to_dc_transform', $form_state['values']['mods_to_dc_transform']);
      variable_set('islandora_csv_to_mods_schema_test_text_xml', $form_state['values']['schema_test_text_xml']);
      variable_set('islandora_csv_to_mods_mods_schema_xsd_version', $form_state['values']['mods_schema_xsd_version']);
      break;

    case 'edit-reset':
      break;
  }
}
