# Islandora CSV to MODS Utility [![Build Status](https://travis-ci.org/ulsdevteam/islandora_csv_to_mods.png?branch=7.x)](https://travis-ci.org/ulsdevteam/islandora_csv_to_mods)
To create MODS from CSV upload and potentially update the related Islandora objects.  Upon upload of CSV file, it is validated and converted to MODS using a two-step process that gives a lot of control to the MODS values that can be created.

## Requirements

* [Islandora](https://github.com/Islandora/islandora)

### Installing
After installing the module, the `admin/islandora/tools/csv_to_mods` configuration will allow a namespace to be defined.  This value defaults to "islandora" and is not needed when the identifier column in the CSV file actually contains full PID values such as "islandora:192".

## CSV Specification
The CSV file should adhere to [RFC 4180 "Common Format and MIME Type for Comma-Separated Values"](https://tools.ietf.org/html/rfc4180).

1. **Headers row**:  the first row of the CSV MUST contain the field names that are represented in the subsequent rows of data.
2. **Only the supported fields will be converted into MODS XML nodes:**
  **Required fields:** identifier, title. 
  **Optional fields:** address, contributor, copyright_status, creator, date, date_digitized, depositor, description, dimension, format, genre, gift_of, normalized_date, normalized_date_qualifier, pub_place, publication_status, publisher, rights_holder, scale, sort_date, source_citation, source_collection, source_collection_date, source_collection_id, source_container, source_id, source_identifier, source_ownership, subject, subject_lcsh, subject_local, subject_location, subject_name, title, type_of_resource.

Each row of the CSV file is converted into a simple flat <sheet> XML file where the nodes each match the CSV field names.  This <sheet> XML file is then transformed using the `transforms/sheet2mods.xsl` file included in this module.

**Note:** If fields are needed that are not currently supported, they can easily be added to the `islandora_csv_to_mods_get_csv_header_xpath_mappings` function in the `includes/utilities.inc` file as well as to be added to the `transforms/sheet2mods.xsl` transform.

## CSV to MODS Usage
From the `islandora/csv_to_mods/import_csv` utility page, upload a CSV file (that adheres to the specifications provided above) and click "Process CSV".

If objects exist in the system at the time of the CSV upload, there will be an option to update these objects with the MODS created from the matching CSV row.  If the "Update All Objects" link is clicked on the "Analysis of uploaded CSV" page, all objects that are referenced by identifiers in the CSV file will be updated with that corresponding row's MODS file.

*If the files are intended to be Imported back into the system, **DO NOT CHANGE the filenames**.*


## Author / License

Written by Brian Gillingham for the [University of Pittsburgh](http://www.pitt.edu).  Copyright (c) University of Pittsburgh.

Released under a license of GPL v2 or later.
