# sqliteman

This is an improved version of pvanek/sqliteman. The main change is that the Item View is fully editable and edits are properly synchronised between the Full View and the Item View. As a consequence of this, it is no longer necessary after editing a field to click on another field before committing. There are also numerous UI improvements and bug fixes. This version only builds for Linux, but it should not be difficult to make it build for other platforms: the only Linux-specific part is the code in the build script which makes a build timestamp.

This version now supports incremental search on a column.
