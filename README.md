ubl_batch_ingest_datastreams
===========================

Batch ingest datastreams

With this module it is possible to add datastreams to items in batch.
Go to the URL https://[islandora-server]/ubl_batch_ingest_datastreams. Here you can upload a ZIP file. This ZIP file should contain at least a CSV file (named the same as the base name of the ZIP file). The CSV file should contain 3 columns:
 - the first column holds an identifier to an item
 - the second column holds the ID of the datastream of that item
 - the third column holds the name of a file that should be added to the given item as the given datastream id

The file at the third column should be included in the Zip file.

If the third column is empty, the datastream identified by the second column can be generated for that item if it does not exist already. This applies only to datastreams with the following IDs: JP2, JPG, TN, PDF, OCR and HOCR. 
Special cases are datastreams identified with HDL or DIMENSIONS: the first will generate a handle for the given item(s). The second will generate the width and height and store it in the RELS-INT if the item contains a JP2 datastream.

If the first column identifies a collection object and the third column is empty, then the specified datastream will be generated for all items in that collection that do not contain this type of datastream.