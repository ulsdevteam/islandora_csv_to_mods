<?php

/**
 * @file
 * Utilities functions for Islandora CSV to MODS Utility module.
 */

set_time_limit(0);

/**
 * Perform transform to convert the <sheet> into <mods>.
 *
 * This will run XML to MODS transformation and save resultant MODS over the
 * original file. This same effect could be achieved using a template system
 * because xslt is relatively slow.  For now, the xslt transform method is
 * used.
 *
 * @param string $xml_filename
 *   The filename to the <sheet> XML file that was created from a single
 *   CSV row.
 *
 * @return string
 *   The filename for the new MODS file
 */
function islandora_csv_to_mods_XMLtoMODS($xml_filename) {
  $xsl = str_replace("/includes", "", dirname(__FILE__) . '/transforms/sheet2mods.xsl');

  $xml_file = file_get_contents($xml_filename);
  $mods = ($xml_file) ? islandora_csv_to_mods_runXslTransform(
            array(
              'xsl' => $xsl,
              'input' => $xml_file,
            )
          ) : '';

  // This file must be deleted in the process function that called this.
  $bytes_written = file_put_contents($xml_filename, $mods);

  return ($bytes_written) ? $xml_filename : '';
}

/**
 * Helper function to perform xsl transformations.
 *
 * This will perform an xsl transformation on the $info that is passed.  This
 * array will contain the xsl to run, as well as the input XML that it runs
 * upon.  The transformed XML will be returned as a string.
 *
 * @param array $info
 *   The array contains the [xsl] the xsl transformation file.
 *   [input] file that needs to be transformed.
 *
 * @return string
 *   The transformed XML.
 */
function islandora_csv_to_mods_runXslTransform($info) {
  $xsl = new DOMDocument();
  $xsl->load($info['xsl']);
  $input = new DOMDocument();
  $input->loadXML($info['input']);

  $processor = new XSLTProcessor();
  $processor->importStylesheet($xsl);
  return $processor->transformToXML($input);
}

/**
 * Validate the uploaded CSV file for use with MODS creation.
 *
 * Helper function that will validate the CSV file for bad date values and is
 * used in the import.form islandora_csv_to_mods_analyze_uploaded_csv function.
 *
 * @param string $filename
 *   The filename to the CSV file that was uploaded.
 *
 * @return bool
 *   Whether or not the CSV file passes validation checks.
 */
function islandora_csv_to_mods_validate_csv($filename) {
  $validation_success = TRUE;
  $obj_files_path = file_directory_temp();
  $csv_file_arr = islandora_csv_to_mods_csv_file_as_array($filename);
  $file_header = (isset($csv_file_arr['header']) ? $csv_file_arr['header'] : array());
  $file_rows = (isset($csv_file_arr['rows']) ? $csv_file_arr['rows'] : array());

  // Get the normalized_date column index.
  $date_index = array_search('normalized_date', $file_header);

  // Get the filename column index.
  $filename_index = array_search('file_name', $file_header);
  // If no 'file_name' is not found, check the other field name 'filename'.
  if ($filename_index === FALSE) {
    $filename_index = array_search('filename', $file_header);
  }

  foreach ($file_rows as $k => $row) {
    // If there is a normalized_date in the header, check this date value.
    if (!($date_index === FALSE)) {
      $normalized_date = $row[$date_index];
      $check = islandora_csv_to_mods_make_custom_csv_dates($row[$date_index], TRUE);
      if ($check == '') {
        $validation_success = FALSE;
        drupal_set_message(t('There is a bad date value for "normalized_date" = "@row_normalized_date" on row #@rownum.  Please upload the CSV after fixing this value.', array('@row_normalized_date' => check_plain($row[$date_index]), '@rownum' => check_plain($k))), 'error');
      }
    }
  }

  return $validation_success;
}

/**
 * Convert the CSV file into an array().
 *
 * Helper function that will load a CSV spreadsheet by name $csv_file, and 
 * return it as an associative array.  This will make sure that the headers
 * are all lowercase.  The array will have two keys -- the [header] will only
 * contain the first row's fieldnames, but the [rows] contains an array
 * of values for each row corresponding to that row's data in the CSV file.
 *
 * @param string $csv_file
 *   filename that points to the CSV file that was uploaded.
 *
 * @return array
 *   'header' => $file_header, 'rows' => $file_rows
 * @throws Exception
 */
function islandora_csv_to_mods_csv_file_as_array($csv_file, $display_rowcount = FALSE) {
  $file_rows = $file_header = array();
  if (file_exists($csv_file)) {
    $row = 0;
    if (($handle = fopen($csv_file, "r")) !== FALSE) {
      try {
        while (($data = fgetcsv($handle)) !== FALSE) {
          $row++;
          if ($row > 1) {
            foreach ($data as $i => $v) {
              $data[$i] = htmlspecialchars(trim($v), ENT_QUOTES);
            }
            $file_rows[] = $data;
          }
          else {
            $tmp_data = $data;
            foreach ($tmp_data as $i => $val) {
              $tmp_data[$i] = str_replace(" ", "_", trim(strtolower($val)));
            }
            $file_header = $tmp_data;
          }
        }
        // Subtract the header row.
        $data_rows = $row - 1;
        if ($display_rowcount) {
          drupal_set_message(t('There are @data_rows rows in the CSV file "@csv_file".',
                array(
                  '@data_rows' => check_plain(number_format($data_rows)),
                  '@csv_file' => check_plain($csv_file),
                )),
                'warning');
        }
      }
      catch (Exception $e) {
        throw new Exception('The file "' . $csv_file . '" could not be parsed as a CSV.');
      }
      fclose($handle);
    }
  }
  return array('header' => $file_header, 'rows' => $file_rows);
}

/**
 * Make CSV date values from value provided.
 *
 * Will handle the CSV values for dates and create a "display date" and a 
 * "sort date" value as best as it can assuming the following table of 
 * possible values and intended display date and sort date values.
 *
 *  CSV value
 *              Display date                      Sort date
 * ---------------------------------------------------------------------------
 *   1945/1955
 *              1945-1955                         1945-01-01T00:00:00-05:00
 * a 1945/1955
 *              ca. 1945-1955                     1945-01-01T00:00:00-05:00
 *   1945-03/1955-04
 *              March 1945- April 1955            1945-03-01T00:00:00-05:00
 * a 1945-03/1955-04
 *              ca. March 1945- April 1955        1945-03-01T00:00:00-05:00
 *   1945-03-05/1955-04-23
 *              March 5, 1945- April 23, 1955     1945-03-05T00:00:00-05:00
 * a 1945-03-05/1955-04-23
 *              ca. March 5, 1945- April 23, 1955 1945-03-05T00:00:00-05:00
 *
 * @param string $short_date
 *   the date value that came from the CSV.
 * @param bool $suppress_display_messages
 *   whether or not to suppress the display messages.
 *
 * @return array
 *   [0] = $sort_date, [1] = $date 
 */
function islandora_csv_to_mods_make_custom_csv_dates($short_date, $suppress_display_messages = FALSE) {
  $short_date = trim($short_date);
  $short_date_length = strlen($short_date);
  $h_format = 'F j, Y';
  $from_date = '';
  // Handle the special cases where there is a date range provided.
  if ($short_date_length == 9 && ($short_date[4] == '/' || $short_date[4] == '-')) {
    $from_date = substr($short_date, 0, 4) . '-01-01';
    $to_date = substr($short_date, 5, PHP_INT_MAX) . '-01-01';
    $h_format = 'Y';
  }
  elseif ($short_date_length == 15 && ($short_date[7] == '/' || $short_date[7] == '-')) {
    $from_date = substr($short_date, 0, 7);
    $to_date = substr($short_date, 8, PHP_INT_MAX);
    $h_format = 'F Y';
  }
  elseif ($short_date_length == 21 && ($short_date[10] == '/' || $short_date[10] == '-')) {
    $from_date = substr($short_date, 0, 10);
    $to_date = substr($short_date, 11, PHP_INT_MAX);
  }
  if ($from_date) {
    $timestamp = strtotime($from_date);
    $timestamp_to_date = strtotime($to_date);
    $display_date = date($h_format, $timestamp) . '-' . date($h_format, $timestamp_to_date);
    return array(date('c', $timestamp), $display_date);
  }
  if (strstr($short_date, '-') == '') {
    if ($short_date_length == 4) {
      $short_date .= '-01-01';
      $h_format = 'Y';
    }
    elseif ($short_date_length == 6) {
      $short_date = substr($short_date, 0, 4) . '-' . substr($short_date, 4, 2) . '-01';
      $h_format = 'F Y';
    }
    elseif ($short_date_length <> 8 && !$from_date) {
      if ($short_date <> '') {
        if (!$suppress_display_messages) {
          drupal_set_message(t('Bad date format for date value of "@short_date".', array('@short_date' => check_plain($short_date))), 'error');
        }
        return array('', '');
      }
      else {
        return array('', '');
      }
    }
  }
  else {
    if ($short_date_length == 7) {
      $short_date .= '-01';
      $h_format = 'F Y';
    }
    elseif ($short_date_length <> 10 && !$from_date) {
      if (!$suppress_display_messages) {
        drupal_set_message(t('Bad date format for date value of "@short_date".', array('@short_date' => check_plain($short_date))), 'error');
      }
      return array('', '');
    }
  }
  $timestamp = strtotime($short_date);
  return array(date('c', $timestamp), date($h_format, $timestamp));
}

/**
 * Fix CSV date for approximate dates.
 *
 * Helper function for creating MODS from CSV - this will inspect the active
 * row for the value in the 'normalized_date_qualifier' field to see whether
 * or not the date field value supplied row is approximate.
 *
 * This does not return anything, but may change the value of variable
 * parameter &$row.
 *
 * @param array $headers
 *   the $headers row from the CSV file.
 * @param array $row
 *   the $row from the CSV file.
 */
function islandora_csv_to_mods_fix_if_csv_date_approximate($headers, &$row) {
  $normalized_date_qualifier_index = array_search('normalized_date_qualifier', $headers);
  $date_index = array_search('date', $headers);
  if ($normalized_date_qualifier_index && isset($row[$normalized_date_qualifier_index])) {
    if ($row[$normalized_date_qualifier_index] == 'yes' || $row[$normalized_date_qualifier_index] == 'approximate') {
      $row[$date_index] = 'ca. ' . $row[$date_index];
    }
  }
}

/**
 * CSV header mappings.
 *
 * This still provides a test as to whether or not a field gets into the
 * initial XML from the sheet - and the headings title that would match for
 * that field.  The related code does not use the xpath value in the current
 * version, but could be used in a subsequent validation test to see
 * that all values have been created in the resultant MODS.
 *
 * @return array
 *   The mappings from CSV field to MODS xpath.
 */
function islandora_csv_to_mods_get_csv_header_xpath_mappings() {
  // TODO:  add /mods/copyright/accessCondition/general.note
  return array(
    'title'                     => '/mods:mods/mods:titleInfo/mods:title',
    'abstract'                  => '/mods:mods/mods:abstract',
    'description'               => '/mods:mods/mods:abstract',
    'copyright_status'          => '/mods:mods/mods:accessCondition/copyrightMD:copyright/@copyright.status',
    'publication_status'        => '/mods:mods/mods:accessCondition/copyrightMD:copyright/@publication.status',
    'rights_holder'             => '/mods:mods/mods:accessCondition/copyrightMD:copyright/copyrightMD:rights.holder/copyrightMD:name',
    'genre'                     => '/mods:mods/mods:genre',
    'identifier'                => '/mods:mods/mods:identifier[@type="pitt"]',
    'source_id'                 => '/mods:mods/mods:identifier[@type="source"]',
    'source_identifier'         => '/mods:mods/mods:identifier[@type="source"]',
    'language'                  => '/mods:mods/mods:language/mods:languageTerm[@type="code" @authority="iso639-2b"]',
    'contributor'               => '/mods:mods/mods:name/mods:namePart[../mods:role/mods:roleTerm[(text()="contributor") and @type="text"]]',
    'creator'                   => '/mods:mods/mods:name/mods:namePart[../mods:role/mods:roleTerm[(text()="creator") and @type="text"]]',
    'depositor'                 => '/mods:mods/mods:name/mods:namePart[../mods:role/mods:roleTerm[(text()="depositor") and @type="text"]]',
    'interviewee'               => '/mods:mods/mods:name/mods:namePart[../mods:role/mods:roleTerm[(text()="interviewee") and @type="text"])',
    'interviewer'               => '/mods:mods/mods:name/mods:namePart[../mods:role/mods:roleTerm[(text()="interviewer") and @type="text"])',
    'address'                   => '/mods:mods/mods:note[@type="address"]',
    'gift_of'                   => '/mods:mods/mods:note[@type="donor"]',
    'date_digitized'            => '/mods:mods/mods:originInfo/mods:dateCaptured',
    'normalized_date'           => '/mods:mods/mods:originInfo/mods:dateCreated[@encoding="iso8601" @keyDate="yes"]',
    'normalized_date_qualifier' => '/mods:mods/mods:originInfo/mods:dateCreated[@qualifier="approximate" @encoding="iso8601" @keyDate="yes"]',
    'date'                      => '/mods:mods/mods:originInfo/mods:dateOther[@type="display"]',
    'display_date'              => '/mods:mods/mods:originInfo/mods:dateOther[@type="display"]',
    'sort_date'                 => '/mods:mods/mods:originInfo/mods:dateOther[@type="sort"]',
    'publisher'                 => '/mods:mods/mods:originInfo/mods:publisher',
    'pub_place'                 => '/mods:mods/mods:originInfo/mods:place/mods:placeTerm[@type="text"]',
    'dimension'                 => '/mods:mods/mods:physicalDescription/mods:extent',
    'format'                    => '/mods:mods/mods:physicalDescription/mods:form',
    'extent'                    => '/mods:mods/mods:physicalDescription/mods:extent',
    'source_collection_id'      => '/mods:mods/mods:relatedItem[@type="host"]/mods:identifier',
    'source_container'          => '/mods:mods/mods:relatedItem[@type="host"]/mods:note[@type="container"]',
    'source_ownership'          => '/mods:mods/mods:relatedItem[@type="host"]/mods:note[@type="ownership"]',
    'source_collection_date'    => '/mods:mods/mods:relatedItem[@type="host"]/mods:originInfo/mods:dateCreated',
    'source_collection'         => '/mods:mods/mods:relatedItem/mods:titleInfo/mods:title',
    'source_citation'           => '/mods:mods/mods:relatedItem/note[@type="prefercite"]',
    'source_other_level'        => '/mods:mods/mods:relatedItem[@type="host"]/mods:note[@type="otherlevel"]',
    'parent_id'                 => '/mods:mods/mods:relatedItem[@type="host"]/mods:identifier',
    'parent_identifier'         => '/mods:mods/mods:relatedItem[@type="host"]/mods:identifier',
    'source_series'             => '/mods:mods/mods:relatedItem[@type="host"]/mods:note[@type="series"]',
    'source_subseries'          => '/mods:mods/mods:relatedItem[@type="host"]/mods:note[@type="subseries"]',
    'subject_geographic'        => '/mods:mods/mods:subject[@authority="lcsh"]/mods:geographic',
    'subject_location'          => '/mods:mods/mods:subject[@authority="lcsh"]/mods:geographic',
    'subject_temporal'          => '/mods:mods/mods:subject[@authority="lcsh"]/mods:temporal',
    'subject_topic'             => '/mods:mods/mods:subject[@authority="lcsh"]/mods:topic',
    'subject_lcsh'              => '/mods:mods/mods:subject[@authority="lcsh"]/mods:topic',
    'subject_topic_local'       => '/mods:mods/mods:subject[@authority="local"]/mods:topic',
    'subject_local'             => '/mods:mods/mods:subject[@authority="local"]/mods:topic',
    'subject_name'              => '/mods:mods/mods:subject[@authority="lcsh"]/mods:name/mods:namePart',
    'scale'                     => '/mods:mods/mods:subject/mods:cartographics/mods:scale',
    'subject'                   => '/mods:mods/mods:subject/mods:topic',
    'type_of_resource'          => '/mods:mods/mods:typeOfResource',
    // These field values are not represented in a MODS file, but will appear
    // in the CSV file.
    'batch' => '',
    'collection' => '',
    'file_name' => '',
    'filename' => '',
    'location' => '',
    // NEW fields that will appear in the CSV file, but not to be transformed to
    // a MODS node.
    'path_to_master_wav' => '',
  );
}

/**
 * For all of the objects specified by the PIDS array, will perform the MODS
 * to DC derivative.
 *
 * This will get the configured value for MODS to DC transform -- and if that
 * is configured, then it will loop over the $pids array to call the derivative
 * creation on each.  Upon completion, this will display a message indicating
 * how many objects' DC were regenerated.
 *
 * @param array $pids
 *   Array of islandora PID values.
 */
function islandora_csv_to_mods_derive_new_dc($pids = array()) {
  module_load_include('inc', 'islandora_csv_to_mods', 'includes/transform.form');
  $updated_count = 0;
  $mods_to_dc_transform = variable_get('islandora_csv_to_mods_mods_to_dc_transform', '');
  if ($mods_to_dc_transform) {
    foreach ($pids as $pid) {
      if ($pid) {
        if (islandora_csv_to_mods_do_mods_2_dc($pid, $mods_to_dc_transform)) {
          $updated_count++;
        }
      }
    }
    drupal_set_message(t('After import of the new MODS, @howmany object DC ' .
        (($updated_count == 1) ? 'was' : 'were') .' updated.',
        array('@howmany' => $updated_count)), 'status');
  }
}

/**
 *
 *
 * @param string $pid
 *   The islandora PID value representing an islandora object.
 * @param string $mods_to_dc_transform
 *   The path to the MODS to DC transform.
 * @return boolean
 *   Whether or not the given object's MODS could be transformed to DC.
 */
function islandora_csv_to_mods_do_mods_2_dc($pid, $mods_to_dc_transform) {
  $islandora_object = islandora_object_load($pid);
  if (is_object($islandora_object)) {
    return (islandora_csv_to_mods_transform_datastream($islandora_object,
        'MODS', 'DC', $mods_to_dc_transform));
  }
}

function islandora_csv_to_mods_get_mods_schema_versions() {
  return array(
    'https://www.loc.gov/standards/mods/v3/mods-3-7.xsd' => 'MODS 3.7 Schema',
    'https://www.loc.gov/standards/mods/v3/mods-3-6.xsd' => 'MODS 3.6',
    'https://www.loc.gov/standards/mods/v3/mods-3-5.xsd' => 'MODS 3.5',
    'https://www.loc.gov/standards/mods/v3/mods-3-4.xsd' => 'MODS 3.4',
    'https://www.loc.gov/standards/mods/v3/mods-3-3.xsd' => 'MODS 3.3',
    'https://www.loc.gov/standards/mods/v3/mods-3-2.xsd' => 'MODS 3.2',
    'https://www.loc.gov/standards/mods/v3/mods-3-1.xsd' => 'MODS 3.1',
    'https://www.loc.gov/standards/mods/v3/mods-3-0.xsd' => 'MODS 3.0'
  );
}

function islandora_csv_to_mods_mods_schema_success($import_mods_filename, $schema_uri) {
  // Assume success.
  $retval = TRUE;

  if ($schema_uri) {
    // First, check the local temp directory to see if the MODS schema has been
    // stored locally.  If not, get it and store it there for quicker access
    // for subsequent schema tests.
    $path_info = pathinfo($schema_uri);
    $local_schema_filename = drupal_realpath(drupal_tempnam("temporary://", $path_info['basename']));
    if (!file_exists($local_schema_filename)) {
      $schema_contents = file_get_contents($schema_uri);
      file_put_contents($local_schema_filename, $schema_contents);
    }
    // Get the datatream from the islandora_object -- and perform the schema check.
    try {
      $mods_content = file_get_contents($import_mods_filename);
      $doc0 = new DOMDocument();
      if (!(@$doc0->loadXML($mods_content) === false)) {
        $retval = ($doc0->schemaValidate($local_schema_filename));
      } else {
        $retval = FALSE;
      }
    }
    catch (Exception $e) {
       die('EXCEPTION in islandora_csv_to_mods_schema_success : called from ' . $_SERVER['SERVER_NAME'] .
    ' - ' . $_SERVER['REQUEST_URI'] . '<pre>
    ' . print_r($e, true));
    }
  }
  return $retval;
}

