# sqliteman

This is an improved version of pvanek/sqliteman. The main change is that the Item View is fully editable and edits are properly synchronised between the Full View and the Item View. As a consequence of this, it is no longer necessary after editing a field to click on another field before committing. There are also numerous UI improvements and bug fixes. This version builds for Linux. It should build on Windows too since I have added a Windows version of the build timestamp, but I do not have a Windows system to test it on.

Any volunteer to build on Windows would be welcome.

This version supports incremental search on a column and a new search widget allowing editing the rows found by the search.
