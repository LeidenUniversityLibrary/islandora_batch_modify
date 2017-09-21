# Islandora Batch Modify
===========================

## Introduction

With this module it is possible to modify islandora objects in batch. You can add new datastreams, generate missing (derivative) datastreams, change the label or owner of an object, set the state of an object, change the label of a datastream or delete a datastream.
Also you can add an object to a collection or remove it from a collection.

### Web interface

Go to the URL `https://[islandora-server]/islandora_batch_modify`. Here you can upload a CSV file or ZIP file. The ZIP file should contain at least a CSV file (named the same as the base name of the ZIP file). The CSV file should be formatted as descripted in the section `CSV format`.

Notice: due to long loading times, it is recommended to use the command line version of this module (see drush below) if there are more than 100 items involved.

### drush

You can also use this module via the command line with drush.

The command you can use is:

> `islandora_batch_modify`

This command needs only 1 option `csvfile` with an absolute path to a csv file.
Use this with a user with appropriate rights.


Examples:
```
 - drush -v --user=admin islandora_batch_modify --csvfile=/tmp/test.csv
 - drush -v --user=admin ibm --csvfile=/tmp/test.csv
```

### CSV format

The provided CSV file (uploaded via the Web Interface directly or part of a ZIP file, or as an option of the drush command) should have 2 or 3 columns. It may have a header row.
The columns must have the following values:

1. An identifier of an Islandora object. This can also be an (unique) identifier within the metadata datastream of an object.
2. This value can be one of these:
   * a valid datastream id, such as MODS, TN, JPG.
   * a property of the object: property:label, property:owner, property:state.
   * a property of a datastream of the object: property:DSID:label, property:DSID:state
   * 'add_to_collection' (for adding the object to a collection)
   * 'remove_from_collection' (for removing the object from a collection)
3. This value can be:
   * empty. This can only be used with a valid datastream id in column 2 which can be generated (so is a derivative).
   * a filename. Use only when there is a valid datastream id in column 2. This file name will be the new datastream for this id:
     * When using the Web interface, this should be a filename that exists within the ZIP file.
     * When using drush this should be an absolute filepath.
   * the value for the property of the object. It should not be empty. In case of property:state the valid values are A, I or D  (Active, Inactive or Deleted).
   * the value for the property of a datastream of the object. It should not be empty. In case of property:DSID:state the only valid value is D (Deleted). This will delete the datastream.
   * a collection id when used with 'add_to_collection' or 'remove_from_collection'. This should be the id of an existing collection.


## Requirements

This module requires the following modules/libraries:

* [Islandora](https://github.com/islandora/islandora)

## Installation
 
Install as usual, see [this](https://drupal.org/documentation/install/modules-themes/modules-7) for further information.
 
## Configuration

No further configuration is needed to use this module. 

## Maintainers/Sponsors

Current maintainers:

* [Lucas van Schaik](https://github.com/lucasvanschaik)

## Development

If you would like to contribute to this module, please contact the current maintainer.

## License

[GPLv3](LICENSE.txt)
Copyright 2017 Leiden University Library

