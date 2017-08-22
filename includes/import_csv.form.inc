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
    '#value' => t('Process CSV')
  );
  
  return $form;
}

function islandora_csv_to_mods_get_uploaded_filename($form_state) {
  $uploaded_file_name = '';
  if (isset($form_state['values']['uploaded_file_name']) && !empty($form_state['values']['uploaded_file_name'])) {
    $uploaded_file_name = $form_state['values']['uploaded_file_name'];
  } elseif (isset($_FILES['files']) && isset($_FILES["files"]["tmp_name"]['file'])) {
    if ($_FILES['files']['error']['file'] == UPLOAD_ERR_OK) {
      $uploaded_file_name = $_FILES["files"]["tmp_name"]['file'];
    }
  }
  return $uploaded_file_name;
}

/**
 * This will process the uploaded CSV file that is stored as the temp file uploaded_file_name
 * and will call islandora_csv_to_mods_prepare_csv2mods to make the MODS.  Depending on
 * which option is checked when this is run, it could potentially update the MODS of objects, or
 * create a ZIP file download of the MODS that get created.
 *
 * @param array $form
 * @param array $form_state
 * @param boolean $make_mods
 * @return type
 */
function islandora_csv_to_mods_review_import_csv_file(array $form, array &$form_state, $make_mods) {
  $uploaded_file_name = '';
  $original_uploaded_file_name = (isset($_FILES["files"]['name']) && isset($_FILES["files"]['name']['file'])) ? $_FILES["files"]['name']['file'] : '';
  $uploaded_file_name = islandora_csv_to_mods_get_uploaded_filename($form_state);

  $analysis = '';
  if ($make_mods) {
    if ($results = islandora_csv_to_mods_prepare_csv2mods($uploaded_file_name)) {
      $analysis = $results['markup'];
      if (isset($results['mods_zip']) && file_exists($results['mods_zip'])) {
        unlink($results['mods_zip']);
      }
    }
    else {
      $analysis = '<h3 class="bad">No results or there were no identifiers in the uploaded CSV file.</h3>';
    }
  }
  else {
    $tmp_name = file_directory_temp() . '/csv_to_mods_' . date('His_hms');
    if ($uploaded_file_name && move_uploaded_file($uploaded_file_name, $tmp_name)) {
      $uploaded_file_name = $tmp_name;
    }
    // Get the analysis HTML markup -- and insert it below the buttons.
    $analysis = islandora_csv_to_mods_analyze_uploaded_csv($uploaded_file_name, FALSE);

    $form['csv_table'] = array(
      '#title' => 'Uploaded uploaded CSV file name',
      '#type' => 'fieldset',
      '#value' => $original_uploaded_file_name,
    );
  }
  $form['uploaded_file_name'] = array(
    '#type' => 'hidden',
    '#default_value' => $uploaded_file_name,
  );
  $form['back'] = array(
    '#type' => 'submit',
    '#value' => t('Start over')
  );
  if (!$make_mods) {
    $form['next'] = array(
      '#type' => 'submit',
      '#value' => t('Make individual MODS'),
    );
  }
  $form['finish'] = array(
    '#type' => 'submit',
    '#value' => t('Finish'),
    '#suffix' => $analysis,
  );

  return $form;
}

function islandora_csv_to_mods_import_csv_form_submit(array $form, array &$form_state) {
  switch ($form_state['stage']) {
    default:
      $form_state['multistep_values'][$form_state['stage']] = $form_state['values'];
      $form_state['new_stage'] = islandora_csv_to_mods_move_to_next_stage($form, $form_state);
     break;
  }

  if ($form_state['triggering_element']['#value'] == 'Finish') {
    if (isset($form['uploaded_file_name']) && isset($form['uploaded_file_name']['#value']) && file_exists($form['uploaded_file_name']['#value'])) {
      unlink($form['uploaded_file_name']['#value']);
    }
    drupal_set_message('Finished processing CSV file.  Ready for another CSV upload.');
    drupal_goto('islandora/csv_to_mods/import_csv');
  }

  if ($form_state['triggering_element']['#value'] == 'Start over') {
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
      return 'stage_upload_csv_file';
     break;

    case 'complete':
      return 'stage_upload_csv_file';
     break;
  }
}

function islandora_csv_to_mods_analyze_uploaded_csv($filename, $make_mods) {
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
    $csv_to_mods_namespace = variable_get('islandora_csv_to_mods_namespace', 'islandora') . ':';
    foreach ($csv_file_arr['rows'] as $row_index => $row) {
      $pid = $csv_to_mods_namespace . $row[$identifier_column];
      $obj = islandora_object_load($pid);
      // The key value will either be TRUE or FALSE based on whether or not the
      // object specified by the identifier exists in Islandora.
      $exists_key = (is_object($obj));
      $objects_exist_state[$exists_key][$pid] = ($exists_key) ? l($pid, 'islandora/object/' . $pid) : $pid;
      $file_rows[] = array('data' => $row, 'class' => ($exists_key) ? array('good') : array('bad'),
        );
    }
  }
  if (!$make_mods && count($objects_exist_state) > 0) {
    $existing_objects = isset($objects_exist_state[TRUE]) ? $objects_exist_state[TRUE] : array();
    $non_existant_obj = isset($objects_exist_state[FALSE]) ? $objects_exist_state[FALSE] : array();
    $markup .= (count($existing_objects) > 0) ? '<h4>Existing objects to be updated by this CSV</h4>[[update_link]]<pre>' . implode(", ", $existing_objects) . '</pre>'
            : '<h4>None of the objects existed.</h4>[[update_link]]';
    $markup .= (count($non_existant_obj) > 0) ? '<h4>Objects in this CSV that do not exist</h4>[[download_link]]<pre>' . implode(", ", $non_existant_obj) . '</pre>'
            : '<h4>None of the objects in this CSV do not exist.</h4>[[download_link]]';
  }

  $base64_temp_csv_filename = base64_encode($filename);
  // Get the HTML for the two buttons, and insert them where they belong.
  $update_url = 'islandora/csv_to_mods/update_objects/all/' . $base64_temp_csv_filename;
  if (drupal_valid_path($update_url, true)) {
    $update_link = l('Update ALL Objects\' MODS', $update_url);
  }
  else {
    $update_link = '<i>You do not have permission to update objects.</i>';
  }
  $download_link = l('Download ALL MODS', 'islandora/csv_to_mods/download_mods/all/' . $base64_temp_csv_filename);
  $markup = str_replace(array('[[update_link]]', '[[download_link]]'), array($update_link, $download_link), $markup);

  if (count($file_rows) > 0) {
    $markup .= theme('table', array('rows' => $file_rows, 'header' => $file_header, 'attributes' => array('class' => array('csv_report_table'))));
  } else {
    $markup .= "There were no rows of data found in the uploaded CSV file.";
  }
  return $markup;
}

/**
 * This will look up the given CSV file's identifier value for the given row_index.
 * @param type $tmp_csv_filename
 * @param type $row_index
 * @return string
 */
function islandora_csv_to_mods_get_mods_row_filename($tmp_csv_filename, $row_index) {
  $csv_file_arr = islandora_csv_to_mods_csv_file_as_array($tmp_csv_filename);
  $file_header = (isset($csv_file_arr['header']) ? $csv_file_arr['header'] : array());
  $file_rows = (isset($csv_file_arr['rows']) ? $csv_file_arr['rows'] : array());

  $adj_row_index = $row_index -1;
  $identifier_column = -1;
  foreach ($file_header as $column_index => $header) {
    if (strtolower($header) == 'identifier' && ($identifier_column < 0)) {
      $identifier_column = $column_index;
    }
  }
  if (isset($file_rows[$adj_row_index]) && isset($file_rows[$adj_row_index][$identifier_column])) {
    if (strstr($file_rows[$adj_row_index][$identifier_column], ":")) {
      return str_replace(":", "_", $file_rows[$adj_row_index][$identifier_column]) . '_MODS.xml';
    }
    else {
      return variable_get('islandora_csv_to_mods_namespace') . "_" . $file_rows[$adj_row_index][$identifier_column] . '_MODS.xml';
    }
  }
  else {
    return 'mods.xml';
  }
}

/**
 * This will parse a row from a CSV file and return an array with two parts to the info:
 *
 * @param type $input_filename
 * @return array
 *   [markup] = HTML
 *   [mods_zip] = filename to the zip download file
 *   [mods_file] = MODS content for the given row (when specific_row is provided)
 */
function islandora_csv_to_mods_prepare_csv2mods($input_filename, $specific_row = NULL) {
  module_load_include('inc', 'islandora_csv_to_mods', 'includes/utilities');
  $markup = '';
  ini_set('auto_detect_line_endings', true);

  // The previous_dir is needed for when done.
  $previous_dir = getcwd();

  // Open csv to read
  $inputFile  = fopen($input_filename, 'rt');

  // Get the headers of the file
  $headers = fgetcsv($inputFile);
  if ((array_search('identifier', $headers) === FALSE) and (array_search('Identifier', $headers) === FALSE)) {
    return FALSE;
  }

  $link_options = array(
      'attributes' => array(
        'target'=> '_blank',
        'title' => '',
      ),
    );
  $working_directory = dirname($input_filename);
  $pathinfo_input_filename = pathinfo($input_filename);
  $working_directory .= '/files_' . $pathinfo_input_filename['filename'];
  $mods_zip = dirname($input_filename) . '/zip_' . $pathinfo_input_filename['filename'] . '.zip';
  // Create a zip archive if not running on a specific row.
  if (is_null($specific_row)) {
    $zip = new ZipArchive;
    $zip->open($mods_zip,ZipArchive::CREATE|ZipArchive::OVERWRITE);
  }

  $csv_header_xpath_mappings = islandora_csv_to_mods_get_csv_header_xpath_mappings();
  $mods_created = 0;

  $csv_to_mods_namespace = variable_get('islandora_csv_to_mods_namespace');
  $existing_pids = array();
  $row_counter = 1;

  // Loop through each row creating a <row> node with the correct data
  while (($row = fgetcsv($inputFile)) !== FALSE) {
    if (!is_null($specific_row) && ($row_counter == $specific_row) || is_null($specific_row)) {
      // First, scan the entire row to get the identifier and the file_name value
      $identifier = '';
      $rowcopy = $row;
      // Set a default filename for this row's MODS... it should be overridden by the
      // row `identifier` reference.
      $outputFilename = 'MODS_' . $row_counter . '.xml';
      foreach($headers as $i => $header) {
        $header = str_replace(" ", "_", strtolower(trim($header)));
        // In case the headers row ends with a few commas like "depositor,,,,," - so must
        // check that the header actually has a value.
        if ($header) {
          if (array_key_exists($i, $row)) {
            // ALL LOWERCASE, replace spaces with "_"
            if ($header == 'identifier') {
              $identifier = trim($row[$i]);
              // If identifier already contains a ":", then do not add the namespace
              if (strstr($identifier, ":")) {
                $outputFilename = str_replace(":", "_", $identifier) . '_MODS.xml'; //  str_replace(array('.tiff', '.tif'), '.xml', $row[$i]);
              }
              else {
                $outputFilename = $csv_to_mods_namespace . "_" . $identifier . '_MODS.xml'; //  str_replace(array('.tiff', '.tif'), '.xml', $row[$i]);
              }
              $mods_directory_path = $working_directory; //  . '/' . $identifier;
              if (!file_exists($mods_directory_path)) {
                mkdir($mods_directory_path);
              }
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
      $pid = $csv_to_mods_namespace . ':' . $identifier;
      $islandora_object = islandora_object_load($pid);
      $exists = is_object($islandora_object);
      if ($exists) {
        $existing_pids[] = $pid;
      }
      $tmp_mods_filename = $working_directory . '/' . $outputFilename;

      $markup .= '<div class="mods_box ' .
              ($exists ? 'good' : 'bad') . '">';
      $markup .= "<h3>CSV row #" . $row_counter . ", PID = " .
              ($exists ? l($pid, 'islandora/object/' . $pid) : $pid). "</h3>";
      // Add a link to download this individual MODS
      $base64_temp_csv_filename = base64_encode($input_filename);
      $link_options['attributes']['title'] = 'Download MODS for ' . $pid;
      if (!$exists) {
        $markup .= l('Download MODS for ' . $pid, 'islandora/csv_to_mods/download_mods/' .
                $row_counter . '/' . $base64_temp_csv_filename, $link_options);
      } else {
        $update_url = 'islandora/csv_to_mods/update_objects/' . $row_counter . '/' . $base64_temp_csv_filename;
        if (drupal_valid_path($update_url, true)) {
          $link_options['attributes']['title'] = 'Update MODS for ' . $pid;
          $update_link = l('Update MODS for ' . $pid, $update_url, $link_options);
        }
        else {
          $update_link = '<i>You do not have permission to update objects.</i>';
        }
        $link_options['attributes']['title'] = 'Download MODS for ' . $pid;
        $markup .= $update_link . ' | ' . l('Download MODS for ' . $pid, 'islandora/csv_to_mods/download_mods/' .
                $row_counter . '/' . $base64_temp_csv_filename, $link_options);
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
      $handle = fopen($tmp_mods_filename, "w");

      fwrite($handle, $strxml);
      fclose($handle);

      // Transform the simple "sheet" xml into MODS.
      $filename = islandora_csv_to_mods_XMLtoMODS($working_directory . '/' . $outputFilename, $identifier);
      $mods_xml = file_get_contents($filename);
      $mods_xmls[] = $mods_xml;
      $markup .= "<blockquote><pre>".htmlspecialchars($mods_xml) ."</pre></blockquote>";
      $markup .= "</div>";
      
      $mods_created += ($filename <> '');
    }
    $row_counter++;

    // do we need to zip the multiple MODS and return the zip?
    if (is_null($specific_row)) {
      $file_pathinfo = pathinfo($filename);
      chdir($file_pathinfo['dirname']);
      $zip->addFile($file_pathinfo['filename'] . '.' . $file_pathinfo['extension']);
    }
    else {
      $mods_zip = '';
    }
  }
  $markup .= "<p>" . number_format($mods_created) . " MODS files created.</p>";
  if (is_null($specific_row)) {
    $zip->close();
  }
  // Delete the working directory because it is all in the Zip archive now.
  $rm_command = 'rm -rf ' . $working_directory;
  $output = array();
  exec($rm_command, $output, $return);
  if ($return) {
    drupal_set_message("There was a problem removing the temporary files at " . $working_directory . '.', "warning");
  }

  // Before returning, set the directory to what it was before this function used
  // the chdir() for making the Zip archive.
  chdir($previous_dir);
  return array('markup' => $markup, 'mods_file' => $mods_xmls, 'mods_zip' => $mods_zip);
}

function islandora_csv_to_mods_update_objects($row_index, $base64_temp_csv_filename) {
  $tmp_csv_filename = base64_decode($base64_temp_csv_filename);
  $csv_to_mods_namespace_only = variable_get('islandora_csv_to_mods_namespace', 'islandora');
  $ns_replace = $csv_to_mods_namespace_only . '_';
  $ns_replace_with = $csv_to_mods_namespace_only . ':';

  if ($row_index == 'all') {
    // Make all MODS -- loop through the CSV and update each MODS for objects that exist.
    if ($results = islandora_csv_to_mods_prepare_csv2mods($tmp_csv_filename)) {
      if (isset($results['mods_file']) && count($results['mods_file']) > 0) {
        foreach ($results['mods_file'] as $idx => $mods_xml) {
          // Adjust the $idx since the csv starts counting at 1, not at 0.
          $this_mods_row_index = $idx + 1;
          $mods_filename = islandora_csv_to_mods_get_mods_row_filename($tmp_csv_filename, $this_mods_row_index);
          $object_pid = str_replace(array("_MODS.xml", $ns_replace), array("", $ns_replace_with), $mods_filename);
          // Load this Islandora object
          $islandora_object = islandora_object_load($object_pid);
          if (is_object($islandora_object)) {
            if (isset($islandora_object['MODS'])) {
              $datastream = $islandora_object['MODS'];
            }
            else {
              $datastream = $islandora_object->constructDatastream('MODS');
            }
            $datastream->setContentFromString($mods_xml);
            $islandora_object->ingestDatastream($datastream);
          }
          else {
            drupal_set_message('The object identified by @object_pid did not exist.', array('@object_pid' => $object_pid), 'error');
          }
        }
      }
    }
  } else {
    if ($results = islandora_csv_to_mods_prepare_csv2mods($tmp_csv_filename, $row_index)) {
      if (isset($results['mods_file'])) {
        $mods_file = array_pop($results['mods_file']);
        // Get the "mods_filename" value that uses the identifier .. translate this
        // to an Islandora PID value.
        $mods_filename = islandora_csv_to_mods_get_mods_row_filename($tmp_csv_filename, $row_index);
        $object_pid = str_replace(array("_MODS.xml", $ns_replace), array("", $ns_replace_with), $mods_filename);

        // Load this Islandora object
        $islandora_object = islandora_object_load($object_pid);
        if (is_object($islandora_object)) {
          if (isset($islandora_object['MODS'])) {
            $datastream = $islandora_object['MODS'];
          }
          else {
            $datastream = $islandora_object->constructDatastream('MODS');
          }
          $datastream->setContentFromString($mods_file);
          $islandora_object->ingestDatastream($datastream);
        }
        else {
          drupal_set_message('The object identified by @object_pid did not exist.', array('@object_pid' => $object_pid), 'error');
        }
      }
    }
  }
}

function islandora_csv_to_mods_download_mods($row_index, $base64_temp_csv_filename) {
  $tmp_csv_filename = base64_decode($base64_temp_csv_filename);
  if ($row_index == 'all') {
    // Make all MODS -- Zip them up, and serve it up zipped.
    if ($results = islandora_csv_to_mods_prepare_csv2mods($tmp_csv_filename)) {
      $markup = $results['markup'];
      $mods_zip = $results['mods_zip'];
      header("Content-type: application/zip");
      header('Content-disposition: attachment; filename='.basename($mods_zip));
      header('Content-Transfer-Encoding: Binary');
      header('Content-Length: ' . filesize($mods_zip));
      readfile($mods_zip);
      // Delete the temp file that was created
      unlink($mods_zip);
      exit;
    }
    $markup = '<h3>No results or the uploaded CSV did not contain any identifiers</h3>';
  } else {
    // Make a single MODS corresponding to the $row_index value -- and serve it up as a single XML file
    if ($results = islandora_csv_to_mods_prepare_csv2mods($tmp_csv_filename, $row_index)) {
      // Inspect the CSV to get the identifier for the given row_index
      $mods_filename = islandora_csv_to_mods_get_mods_row_filename($tmp_csv_filename, $row_index);
      header("Content-type: text/xml");
      header("Content-Disposition: attachment; filename=" . $mods_filename);
      header("Pragma: no-cache");
      header("Expires: 0");
      print array_pop($results['mods_file']);
      if (file_exists($mods_filename)) {
        // Delete the temp file that was created
        unlink($mods_filename);
      }
      exit;
    }
    else {
      drupal_set_message('There was a problem attempting to download the MODS for @row_index given the CSV file (temporary file: @csv_file)', array('@row_index' => $row_index, '@csv_file' => $tmp_csv_filename));
    }

    return;
  }
}
