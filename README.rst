PerpetuumPlanner
================

Character planner and item database for the Perpetuum_ MMORPG.

.. _Perpetuum: http://www.perpetuum-online.com/

Author: Nikita Nemkin <nikita@nemkin.ru>

Homepage: http://www.perpetuum-planner.com


License
-------

PerpetuumPlanner is licensed under the GNU GPL v3 license.

Libraries used are under the GNU LGPL v2.1.

Game data is used without any specific license.


Game data
---------

Game data is distributed separately (see project downloads).
It is a set of text files in JSON-like format native to the client.
Parser and converters for this format can be found in `data/perpetuum.py` and `data/tools.py`.

Data is split into `fragments`, `translations` and  `icons`.
Fragments are pieces of data with uniform structure (much like SQL tables), translations
and icons are self-explanatory.

Note 1: Included icons are the recompressed (for smaller size) low-quality versions
used in the Planner. Originals can be extracted from `Perpetuum.gbf`.

Note 2: At any given time there is only one translation dictionary in `Perpetuum.gbf`.
To extract another language version you need to change the language and restart Perpetuum
(notice how it downloads a new dictionary).

The GBF unpacker script is `data/tools.py`. It requires Python 2.7 to run. To unpack
the whole `Perpetuum.gbf` use::

    python tools.py -i path\to\perpetuum.gbf -o output\dir

If you're only interested in the game data, download the latest `planner_data_YYYY_MM_DD.zip`
from the downloads section and have fun.


Build procedure
---------------

You need:

* Python 2.7, Qt 4.7 binaries, Qt Creator.
* Some experience with C++, Pyhton and Qt.

Official builds of PerpetuumPlanner are created with VisualStudio 2008.
Qt version used is the latest 4.7 from git (http://qt.gitorious.org/) configured and built according to `meta/config_skin.cache`
with the ``/MT`` flag patched into win32-msvc2008 mkspec (``QMAKE_CFLAGS_RELEASE = -O2 -MT``).
(The goal is relatively small and portable binary without external dependencies).

Steps:

1. Fetch the project from git.
2. Download `planner_data_YYYY_MM_DD.zip` from the downloads section and unpack it into the project's `data` directory.
3. Get the `MyriadPro-BoldCond.otf` (Myriad Pro Cond font) somewhere and put it in the res directory.
   It's a commercial font from Adobe with draconian license. I'm not sure it's legal to embed it even if you buy it.
   Therefore it's not included. To build without it, remove corresponding line from res/resources.qrc.
4. In the data directory run::

       python compile.py

   This command parses data fragments and converts them into serialized `QVariant` blobs which are
   written as `data/compiled/*.dat` files. Apart from throwing out some unused definitions, no changes are made during conversion.
   Later these `*.dat` filed (and also icons from the `data/icons`) are compiled into the Planner executable.

   If you have PyYAML installed this command also creates `game.yaml` file which us handy for debugging data errors.

5. Download QtCreator, open perpetuumplanner.pro, set up your Qt paths in the settings and build.


Coding style
------------

The coding style used is more or less the `Qt coding style`__.

.. __: http://qt.gitorious.org/qt/pages/QtCodingStyle
