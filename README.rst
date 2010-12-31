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

Game data comes from two sources. Until version 1.2 all the data came from the OCRed screenshots.
(A couple of thousand of them, mostly of item information windows).
Starting from version 1.2 I've unpacked `Perpetuum.gbf` and got translation strings and HQ icons from there.
To my great disappointment, `Perpetuum.gbf` did not contain item attributes, components and requirements.

Language independent data is stored in the YAML format in the `data/yaml` directory. Translation keys serve as identifiers.
Translation dictionaries, extracted from the .gbf, can be found in the `data/re` directory.
Along with dictionaries there are some text files that were used to build and clean up the yaml dataset and might prove useful in the future.

Note: at any given time there is only one dictionary in the `Perpetuum.gbf`. To extract another language version
you should change the language in the options, restart Perpetuum and let it download the new dictionary.

The unpacker script is `data/unpack.py`. It requires Python 2.7 to run. To unpack a dictionary use::

    python -f path\to\perpetuum.gbf -s dictionary path\to\output\dir

To unpack the whole `Perpetuum.gbf` use::

    python -f path\to\perpetuum.gbf dictionary path\to\output\dir

If you're only interested in the game data, download `planner_data.zip` from the downloads section and have fun.
The icons in `planner_data.zip` are recompressed for smaller size. If you want HQ originals (with 8bit alpha),
unpack them yourself from the gbf.


Build procedure
---------------

Note: you should have some experience with Pyhton and Qt.

1. Fetch the project from git.
2. Download `planner_data.zip` from the downloads section and unpack it into the project directory.
3. Get the `MyriadPro-BoldCond.otf` (Myriad Pro Cond font) somewhere and put it in the res directory.
   It's a commercial font from Adobe with draconian license. I'm not sure it's legal to embed it even if you buy it.
   Therefore it's not included. To build without it, remove corresponding line from res/resources.qrc.
4. Install Python 2.7, PyYAML and PyQt. (They are used to prepare the game data.)
5. In the data directory run::

       python compile.py

   This command combines language-independent game data from the `data/yaml` directory and serializes it as QVariant into the `data/compiled/game.dat`.
   It also takes the `data/re/dictionary_*.txt` files with translated strings and creates `data/compiled/lang_*.dat` files.
   (To have an idea of what's stored in the QVariant serialized `.dat` files have a look at the corresponding `.yaml` files.)
6. Download and install Qt.
   The "official" builds of PerpetuumPlanner are done with VisualStudio 2008.
   Qt version used is the latest 4.7 from git (http://qt.gitorious.org/) configured and built according to `meta/config_skin.cache`
   with an ``/MT`` flag patched into win32-msvc2008 mkspec (``QMAKE_CFLAGS_RELEASE = -O2 -MT``).
7. Download QtCreator, open perpetuumplanner.pro, set up your Qt paths in the settings and build.


Coding style
------------

The coding style used is more or less the `Qt coding style`__.

.. __: http://qt.gitorious.org/qt/pages/QtCodingStyle


Code notes
----------

TBD
