<?php

function islandora_csv_to_mods_import_csv_form(array $form, array &$form_state) {
  module_load_include('inc', 'islandora_csv_to_mods', 'includes/utilities');

  if (!isset($form_state['stage'])) {
    $form_state['stage'] = 'stage_upload_csv_file';
  }

  switch ($form_state['stage']) {
    case 'stage_upload_csv_file':
      return islandora_csv_to_mods_import_csv_pickfile($form, $form_state);
     break;

    case 'stage_review_uploaded_csv':
      return islandora_csv_to_mods_review_import_csv_file($form, $form_state, FALSE);
     break;

    case 'stage_ok_update_objects':
      return islandora_csv_to_mods_review_import_csv_file($form, $form_state, TRUE);
     break;
  }
}

function islandora_csv_to_mods_import_csv_pickfile($form, &$form_state) {
  $markup_import = '<p>' . t('This will allow a CSV file to be uploaded and each row would be converted to MODS ' .
          'that could potentially update the objects (referred to by the `identifier` column) directly, ' .
          'or downloaded.') . '</p>';

  // Prepare a couple variables for use with file upload options:
  $upload_size = min((int) ini_get('post_max_size'), (int) ini_get('upload_max_filesize'));
  $extensions = array('csv');

  $form['importform'] = array(
    'form_description' => array('#markup' => $markup_import),

    'file' => array(
      '#type' => 'file',
      '#title' => t('File'),
      '#description' => t('Select CSV file.<br/>Files must be less than <strong>@size MB.</strong><br/>Allowed file types: <strong>@ext.</strong>', array('@size' => $upload_size, '@ext' => $extensions[0])),
      '#upload_location' => 'temporary://',
      '#upload_validators' => array(
        'file_validate_extensions' => $extensions,
         // Assume its specified in MB.
        'file_validate_size' => array($upload_size * 1024 * 1024),
      ),
    ),
  );
  $form['next'] = array(
    '#type' => 'submit',
    '#value' => t('Next')
  );
  
  return $form;
}

/**
 * This will process the uploaded CSV file that is stored as the temp file uploaded_file_name
 * and will call islandora_csv_to_mods_prepare_csv2mods to make the MODS.  Depending on
 * which option is checked when this is run, it could potentially update the MODS of objects, or
 * create a ZIP file download of the MODS that get created.
 *
 * @param array $form
 * @param array $form_state
 * @param type $do_push
 * @return type
 */
function islandora_csv_to_mods_review_import_csv_file(array $form, array &$form_state, $do_push = FALSE) {
  dpm('_________ ' . $form_state['stage']);
  $uploaded_file_name = '';
  $original_uploaded_file_name = (isset($_FILES["files"]['name']) && isset($_FILES["files"]['name']['file'])) ? $_FILES["files"]['name']['file'] : '';
  if (isset($form_state['values']['uploaded_file_name']) && !empty($form_state['values']['uploaded_file_name'])) {
    $uploaded_file_name = $form_state['values']['uploaded_file_name'];
  } elseif (isset($_FILES['files']) && isset($_FILES["files"]["tmp_name"]['file'])) {
    if ($_FILES['files']['error']['file'] == UPLOAD_ERR_OK) {
      $uploaded_file_name = $_FILES["files"]["tmp_name"]['file'];
    }
  }

  if ($do_push) {
    if ($results = islandora_csv_to_mods_prepare_csv2mods($uploaded_file_name)) {
      $analysis = $results['markup'];
    }
    else {
      $analysis = '<h3 class="bad">No results</h3>';
    }
  }
  else {
    $tmp_name = file_directory_temp() . '/csv_to_mods_' . date('His_hms');
    if ($uploaded_file_name && move_uploaded_file($uploaded_file_name, $tmp_name)) {
      $uploaded_file_name = $tmp_name;
    } else {
      if ($do_push && !$uploaded_file_name) {
        drupal_set_message('There was an error uploading the archive [' . $uploaded_file_name . ' :: ' . $tmp_name . '] file.');
      }
    }

    // Get the analysis HTML markup -- and insert it below the buttons.
    $analysis = islandora_csv_to_mods_analyze_uploaded_csv($uploaded_file_name, $do_push);

    $form['csv_table'] = array(
      '#title' => 'Uploaded uploaded CSV file name',
      '#type' => 'fieldset',
      '#value' => $original_uploaded_file_name,
    );
    $form['uploaded_file_name'] = array(
      '#type' => 'hidden',
      '#default_value' => $uploaded_file_name,
    );
  }
  $form['back'] = array(
    '#type' => 'submit',
    '#value' => t('Back')
  );
  $form['next'] = array(
    '#type' => 'submit',
    '#value' => ($do_push ? t('Finish') : t('Next')),
    '#suffix' => $analysis,
  );
  return $form;
}

function islandora_csv_to_mods_import_csv_form_submit(array $form, array &$form_state) {
  dpm($form_state['stage']);
  switch ($form_state['stage']) {
    case 'complete':
      $form_state['multistep_values'][$form_state['stage']] = $form_state['values'];
      if ($form_state['triggering_element']['#value'] != 'Back') {
        islandora_csv_to_mods_complete_submit($form, $form_state);
        $form_state['complete'] = TRUE;
      }
     break;

    default:
      $form_state['multistep_values'][$form_state['stage']] = $form_state['values'];
      $form_state['new_stage'] = islandora_csv_to_mods_move_to_next_stage($form, $form_state);
     break;
  }

  if (isset($form_state['complete'])) drupal_goto('complete-page');

  if ($form_state['triggering_element']['#value'] == 'Back') {
    $form_state['new_stage'] = islandora_csv_to_mods_move_to_previous_stage($form, $form_state);
  }

  if (isset($form_state['multistep_values']['form_build_id'])) {
    $form_state['values']['form_build_id'] = $form_state['multistep_values']['form_build_id'];
  }
  $form_state['multistep_values']['form_build_id'] = $form_state['values']['form_build_id'];
  $form_state['stage'] = $form_state['new_stage'];
  $form_state['rebuild'] = TRUE;
}

function islandora_csv_to_mods_move_to_next_stage($form, &$form_state) {
  switch ($form_state['stage']) {
    case 'stage_upload_csv_file':
      return 'stage_review_uploaded_csv';
     break;

    case 'stage_review_uploaded_csv':
      return 'stage_ok_update_objects';
     break;

    case 'stage_ok_update_objects':
      return 'complete';
     break;
  }
}

function islandora_csv_to_mods_move_to_previous_stage($form, &$form_state) {
  switch ($form_state['stage']) {
    case 'stage_review_uploaded_csv':
      return 'stage_upload_csv_file';
     break;

    case 'stage_ok_update_objects':
      return 'stage_review_uploaded_csv';
     break;

    case 'complete':
      return 'stage_ok_update_objects';
     break;
  }
}

function islandora_csv_to_mods_analyze_uploaded_csv($filename, $do_push) {
  module_load_include('inc', 'islandora_csv_to_mods', 'includes/utilities');

  $markup = '<h3>Analysis of uploaded CSV</h3>';
  $validation_success = islandora_csv_to_mods_validate_csv($filename);
  if (!$validation_success) {
    $markup .= "<b>Validation of the CSV file failed.</b>";
  }

  $csv_file_arr = islandora_csv_to_mods_csv_file_as_array($filename);
  $file_header = (isset($csv_file_arr['header']) ? $csv_file_arr['header'] : array());
  $file_rows = array();

  $identifier_column = -1;
  foreach ($file_header as $column_index => $header) {
    if (strtolower($header) == 'identifier' && ($identifier_column < 0)) {
      $identifier_column = $column_index;
    }
  }
  $objects_exist_state = array();
  if ($identifier_column > -1) {
    $object_namespace = variable_get('islandora_csv_to_mods_namespace', 'islandora') . ':';
    foreach ($csv_file_arr['rows'] as $row_index => $row) {
      $pid = $object_namespace . $row[$identifier_column];
      $obj = islandora_object_load($pid);
      // The key value will either be TRUE or FALSE based on whether or not the
      // object specified by the identifier exists in Islandora.
      $exists_key = (is_object($obj));
      $objects_exist_state[$exists_key][$pid] = ($exists_key) ? l($pid, '/islandora/object/' . $pid) : $pid;
      $file_rows[] = array('data' => $row, 'class' => ($exists_key) ? array('good') : array('bad'),
        );
    }
  }
  if (!$do_push && count($objects_exist_state) > 0) {
    $existing_objects = isset($objects_exist_state[TRUE]) ? $objects_exist_state[TRUE] : array();
    $non_existant_obj = isset($objects_exist_state[FALSE]) ? $objects_exist_state[FALSE] : array();
    $markup .= (count($existing_objects) > 0) ? '<h4>Existing objects to be updated by this CSV</h4><pre>' . implode(", ", $existing_objects) . '</pre>'
            : '<h4>None of the objects existed.</h4>';
    $markup .= (count($non_existant_obj) > 0) ? '<h4>Objects in this CSV that do not exist</h4><pre>' . implode(", ", $non_existant_obj) . '</pre>'
            : '<h4>None of the objects in this CSV do not exist.</h4>';
  }

  // dpm($file_rows);
  if (count($file_rows) > 0) {
    $markup .= theme('table', array('rows' => $file_rows, 'header' => $file_header, 'attributes' => array('class' => array('csv_report_table'))));
  } else {
    $markup .= "There were no rows of data found in the uploaded CSV file.";
  }
  return $markup;
}

/**
 * This will parse a row from a CSV file and return an array with two parts to the info:
 *
 * @param type $input_filename
 * @return array
 *   [markup] = HTML
 *   [identifiers_and_filepaths][path] = string
 *   [identifiers_and_filepaths][identifier] = string
 */
function islandora_csv_to_mods_prepare_csv2mods($input_filename) {
  module_load_include('inc', 'islandora_csv_to_mods', 'includes/utilities');
  $markup = '';
  ini_set('auto_detect_line_endings', true);
  $outputFilename   = 'MODS.xml';

  // Open csv to read
  $inputFile  = fopen($input_filename, 'rt');

  // Get the headers of the file
  $headers = fgetcsv($inputFile);
  if ((array_search('identifier', $headers) === FALSE) and (array_search('Identifier', $headers) === FALSE)) {
    return FALSE;
  }

  $working_directory = dirname($input_filename);

  $markup .= "<h3>Working directory " . $working_directory . "</h3>";

  $row_counter = 1;

  $csv_header_xpath_mappings = islandora_csv_to_mods_get_csv_header_xpath_mappings();
  $mods_created = $files_copied = $bytes_written = 0;

  $ingest_namespace = variable_get('islandora_csv_to_mods_namespace');
  $identifiers_and_filepaths = array();
  // Loop through each row creating a <row> node with the correct data
  while (($row = fgetcsv($inputFile)) !== FALSE) {
    // First, scan the entire row to get the identifier and the file_name value
    $row_file_name = $identifier = '';
    $rowcopy = $row;
    foreach($headers as $i => $header) {
      $header = str_replace(" ", "_", strtolower(trim($header)));
      // In case the headers row ends with a few commas like "depositor,,,,," - so must
      // check that the header actually has a value.
      if ($header) {
        if (array_key_exists($i, $row)) {
          // ALL LOWERCASE, replace spaces with "_"
          if ($header == 'identifier') {
            $identifier = trim($row[$i]);
            $mods_directory_path = $working_directory . '/' . $identifier;
            if (!file_exists($mods_directory_path)) {
              mkdir($mods_directory_path);
            }
          }
          elseif (($header == 'file_name' || $header == 'filename') && array_key_exists($i, $row)) {
            $row_file_name = $working_directory . '/' . $row[$i];
            $outputFilename = str_replace(array('.tiff', '.tif'), '.xml', $row[$i]);
          }
          elseif ($header == 'normalized_date' && array_key_exists($i, $row)) {
            list($sort_date, $date) = islandora_csv_to_mods_make_custom_csv_dates($row[$i], FALSE);
            // Add the sort_date field and solr 'date' field
            if ((array_key_exists('sort_date', $headers) === FALSE)) {
              $headers[] = 'sort_date';
              $rowcopy[] = $sort_date;
            }
            if ((array_key_exists('date', $headers) === FALSE)) {
              $headers[] = 'date';
              $rowcopy[] = $date;
            }
            islandora_csv_to_mods_fix_if_csv_date_approximate($headers, $rowcopy);
          }
        }
      }
      $headers[$i] = $header;
    }
    $pid = $ingest_namespace . ':' . $identifier;
    $islandora_object = islandora_object_load($pid);
    $exists = is_object($islandora_object);
    $markup .= '<div class="mods_box ' .
            ($exists ? 'good' : 'bad') . '">';
    $markup .= "<h3>row " . $row_counter . ", PID = " .
            ($exists ? l($pid, '/islandora/object/' . $pid) : $pid). "</h3>";

    // MOVE the file into the row's working directory
    if ($row_file_name) {
      if (file_exists($row_file_name)) {
        $command = "cp $row_file_name $working_directory" . '/' . $identifier . '/.';
        exec($command);
        $markup .= "COPY $row_file_name to $working_directory" . '/' . $identifier ."/.<br>";
        $files_copied++;
      }
    }

    // Create a new dom document with pretty formatting
    $doc  = new DomDocument();
    $doc->formatOutput   = true;

    // Start creating the MODS XML by adding the root node to the document.
    $root = $doc->createElement('sheet'); // mods:mods'); // ,'test value');
    $root->appendChild($doc->createAttribute('xmlns:xsi'))->appendChild($doc->createTextNode("http://www.w3.org/2001/XMLSchema-instance"));

    $root = $doc->appendChild($root);
    $child = NULL;
    $depositor_in_CSV = FALSE;
    foreach($headers as $i => $header) {
      if (array_key_exists($header, $csv_header_xpath_mappings)) {
        if (is_array($rowcopy) && array_key_exists($i, $rowcopy) && $rowcopy[$i]) {
          $child = $doc->createElement($header);
          $child = $root->appendChild($child);
          $value = $doc->createTextNode($rowcopy[$i]);
          $value = $child->appendChild($value);
          if (!$depositor_in_CSV && $header == 'depositor') {
            $depositor_in_CSV = TRUE;
          }
        }
      }
    }
    if ($child) {
      $root->appendChild($child);
    }

    $strxml = $doc->saveXML();

    $markup .= "<blockquote><pre>".htmlspecialchars($strxml) ."</pre></blockquote>";

    $handle = fopen($working_directory . '/' . $identifier . '/' . $outputFilename, "w");
    $bytes_written += fwrite($handle, $strxml);
    fclose($handle);
    $markup .= "</div>";
    $filename = islandora_csv_to_mods_XMLtoMODS($working_directory . '/' . $identifier . '/' . $outputFilename);
    $mods_created += ($filename <> '');
    $markup .= "<p>created MODS from spreadsheet row = <b>" . $filename . "</b></p>";
    $path_parts = pathinfo($filename);

    $identifiers_and_filepaths[] = array('path' => $path_parts['dirname'], 'identifier' => $ingest_namespace . $identifier);
    $row_counter++;
  }
  $markup .= "<p>" . number_format($files_copied) . " files copied to incoming directory <b>" . $working_directory . "</b></p>";
  $markup .= "<p>" . number_format($mods_created) . " MODS files created.</p>";

  return array('' => $identifiers_and_filepaths, 'markup' => $markup);
}
