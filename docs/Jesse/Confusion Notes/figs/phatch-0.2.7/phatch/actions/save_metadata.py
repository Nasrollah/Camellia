# Phatch - Photo Batch Processor
# Copyright (C) 2007-2008 www.stani.be
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see http://www.gnu.org/licenses/
#
# Phatch recommends SPE (http://pythonide.stani.be) for editing python files.

# Embedded icon is designed by Igor Kekeljevic (http://www.admiror-ns.co.yu).

# Follows PEP8

from core import ct, models, pil
from lib.reverse_translation import _t

#no need to lazily import these as they are always imported
import shutil
import os


class Action(models.LosslessSaveMixin, models.Action):
    """Defined variables: <filename> <type> <folder> <width> <height>"""

    label = _t('Save Tags')
    author = 'Stani'
    email = 'spe.stani.be@gmail.com'
    version = '0.1'
    tags = [_t('file'), _t('metadata')]
    __doc__ = _t('Save only metadata (lossless)')

    def apply(self, photo, setting, cache):
        info = photo.info
        filename = self.get_lossless_filename(photo, info)
        #do it
        if info['path'] != filename:
            shutil.copy2(info['path'], filename)
        info.save(filename)
        if photo.modify_date:
            # Update file access and modification date
            os.utime(filename, (photo.modify_date, photo.modify_date))
        return photo

    icon = \
'x\xda\x01\xc7\x108\xef\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x000\x00\
\x00\x000\x08\x06\x00\x00\x00W\x02\xf9\x87\x00\x00\x00\x04sBIT\x08\x08\x08\
\x08|\x08d\x88\x00\x00\x10~IDATh\x81\xd5\x9a{pT\xd7}\xc7?\xe7\xbe\xf6\xa5\
\xd5cWZIHH \x81$\x90\x01\x81\xb10\xe6Yc\x9c\x80\xf1\xd83\xb1\xf3p\x92\xdaq\
\x13\xa7\xd3q\xa6N\x93N\x86N\xc6&L\xd2\xbam\xdc\x8c\x93\x19\xbb\xa9I\x1fv\
\xe2\xc4C\x8d\xc7`b\x9b\x180X&`@\x8f !@\xef\xc7\xea\xb9Z\xad\xa4\x95\xb4\xda\
\xdd\xbb\xa7\x7f\xdc\xd5"\x81\x1d?p\xd3\xe9\x999\xa3\xab\x1d\xad\xee\xf7\xf3\
\xfb}\x7f\xbfs\xee\xd9\x15RJ\xfe?\x0f\xe5\xffZ\xc0\x8d\x0e\xedF\xde\xfc\xf9\
\xcf\x7f\xbeh\xdb\xb6mO\xe7\xe5\xe5U\xa5\xa5\xa5\xe5\x99\xa6)GGG{\xfc~\xff\
\xd9\x9a\x9a\x9a\xbf9p\xe0\xc0\xd0\xa7%\xf4\x83\x86\xf8\xa4\x16z\xfc\xf1\xc7\
\x9f\xaa\xae\xae\xfeVSS\x93\xde\xdc\xdc\xcc\xe0\xe0`PJ\x89\xcf\xe7\xf3,Y\xb2\
\x84\xe2\xe2\xe2\xc8\xd0\xd0\xd0\xd1\xd5\xabWgG\xa3\xd1/\xec\xdc\xb9\xb3\xeb\
S\xd6\x0e|D\x00!\x84\xc0\xb2\x9b\x02\xf0\xd8c\x8f\xfd\xe0\xe6\x9bo\xde\xfd\
\xf2\xcb/\x0f755\xbd544\xd4=66\x16\x02"v\xbb=\xa3\xac\xacl\xcd\xd2\xa5Kwl\
\xda\xb4I\xf7x<\x94\x94\x94L\x8c\x8d\x8d}e\xe7\xce\x9d\x07\xff\xe4\x00B\x08\
\x05\xb0\x03\xde\x15\x7f\xcf\xce\xac\xde\xe5y_\xa9z\xec\xf1W_}\xb5\xe7\xd8\
\xb1c\xafLOO\x9f\x03\xba\x81\x11 \xf6\xb5\xaf}\xadb\xfd\xfa\xf5\xff\xd6\xdf\
\xdf\x9f\xd7\xd2\xd2BEE\x05\xb7\xdez+\x19\x19\x192\x14\n\xfdL\xd3\xb4\xefl\
\xdd\xba5\xfe\'\x01HF^\x07\x8a\xb7\x9e\xe0eG.\x95y\'\xefP\xb2\xfb*\xa9\xdf\
\xf9\xd3\xa8\xd0\xe5$\x10\x93\x12)$R\x93v\x87[f\xa7o\xe8{D\xacH\xdfDSS\x13\
\x97/_\xa6\xa8\xa8\x88m\xdb\xb6!\xa5dbb\xa2vjj\xea\x9e\x1d;v\xf4~\x1a\x00\
\x1f\xd6\x85\x04`Cc\xa13\x9fe\x86\x07\xc59\x9a\x87\xdfS\x8b\xd0\xa5\xe1( \
\xcbQ\x80\xcfYH\xaec!yzQ$#R\xdc+\x8e\xde\xb2\x87\x83\xe1\xa7Y\xbcx1\x86a\xd0\
\xdd\xdd\x8d\x94\x12!\x04N\xa7s\x8d\xdb\xed\xbep\xe4\xc8\x91\xbb\xfed\x00\
\xf6\\\n\xb4\x0c4)!\xdc\x8a\xd9\xf4\xcb\x91\xfa\xdaG\xd9\xdf\xf9<\x11\xe4\
\xb5oPP5\x85\xeeu\xbf\xe57\x99\xdf\xc5\xe5\xb1\x13\x0c\x06\x89\xc7\xe3\xccf[\
\xd3\xb4L\x97\xcbu\xe8\xad\xb7\xde\xfa\xf1\xfe\xfd\xfb\xd5\xffM\x00\x15p\x97\
\xfd5\x0f\x08\x01\x93\x1d\xc4\x87zG\xfa\xf2<\x0b\x07\x02\xa7yA?\xb8\xea\xe4d\
\xfb\x1c\xe1hhBO\xcdXY/Wv>\xcf\x94m\x84\xe1\xe1a\xa2\xd1(\xa6i"\xa5DJ)t]\xff\
\x8e\xc7\xe39\xf9\xe6\x9bo\xe6\x7fR\x80\x0f\xac\x81d\xf1\xba\x1cE\xac\\\xf3\
\x13~\x97\xbd\t\x874a\xe8\xc1\xaa\xf3>{Q\xf9\xf2\xe5\xcb\xff0\xe5\x1c\xb9\
\xad\xeb\xeb\xfbP5\x15E\xa8((\x08\xac\x9f\n*B(\x08\x04"\xae\xe2=\xbd\x9e\xcd\
\xf2A\xb2\xb2\xb2P\x14\x85D"\x81\x94\x12\xd341Ms8\x16\x8b}\xe9\xae\xbb\xee:\
\xfai\x02\xe8\x80o\xd5O\xf8az9\x0fy\xd6Z\xaf\xcbi\x95\xc8\x81\x85\xd2\x96/\
\x85\xb2\xa9\x17MW\x92\xe2\xe7L\xa1"P\x10I\x14\x81\xb0\xae\xaf\xe4R\xdd\xfe0\
\x05\x9eE\xd8l\xb6\x14@"\x91 \x91H$L\xd3\xfcAmm\xed\x0f\x9fx\xe2\x89\xc4\r\
\x01$\xbb\x8f\xc3Y\xca\xb2[~\xce1$\xe9\x99\xab\xe6\xff\xcd\xacP\x15m\x9e\xf0\
\x99>\x95\xdaGT\x96}[!w\x9b\x82"\x94$\x8c\x00\x04\x89\xa0A\xf1\xe9\xbb(\xb7\
\xaf\xc3\xe5r\x01\xa4\xb2\x91\xbc\xf7\xef\x14E\xf9\xf2\x8e\x1d;\x86?\n\xc0\
\x07\xd5\x80\x004\xbb\x97l\x144E\xbf^\xbc*44tT4T\x92\xbeG\xc7\xbd@\xc7\xb7N\
\xa7\xf1{:\xad?6Pb\x06*:*\x06\x1a\x066\x8f\xa0\xff\xce\xdf\xf2\xae\xfa+\x06\
\x06\x06\x98\x9e\x9e&\x91\xb8\x1ap)\xe5v\xd34\xeb\x0f\x1f>\xbc\xe9F\x00$\x90\
\x08\xd6\x11\x18{\xd3uRhs\xdf`\x89\xb7\x84k\xa8BO\xfe\xae3\xdd\xaaS\xf7u\x9d\
\x89\x06\x9d5\xffd\x10\xeb\xd5\xe9{IG\x13\xc6\xbc\xe2\xd6u\x03\xb6vr\xbe\xe2\
\x17t\x07\xda\x88D"\xf3 \x80\x05R\xcac\x87\x0f\x1f\xfe^2\x98\x1f8\xfeX\r\x18\
{\xf6\xec\xf9\x86\xa2*O\x9dX\xf03\x9b\xb9h\x98\xac\x15\xf3\xc5+\xc9k"\x1a\
\xed\xcf\xa8\x0c\x1c\xd4(yP\xa5\xe4\xab*\x9a\xa1"#\n\xc13\n\xc1\x1a\x85\x9c\
\xcd\n\xbeM\x96\x8d,E\x02\x90DG\x05\x99\xc7o\xa5\xcc^\x8d\xcb\xe5BUU,\x07\
\xa7\xc6a]\xd7\xff\xfc3\x9f\xf9L\xf0#\x03\xec\xdf\xbf\xdfp\xbb\xdd?s8\x1c\
\x8f\x0c\x0c\x0ep\xa6\xf58uU\xfb0#\x92\xdc\xf5v\x0c]O\x89\x9f\x9d\xfd\xafj\
\xe4\xacS\xb1gh\x04\xdfU\x19\xa9Q\x99jU\xc8X\xae2zF\xa1\xf0\x1eA\xe97\x94\
\x94\xfc\xab\xa9\x96$\xe2\x92DM\x01\xcbF>\x8b\xc7\xe3A\xd3\xb4k!\xba\xa5\x94\
_\xb8\xfb\xee\xbbO_\xabU\xdd\xb3g\xcf\xbc\x17\x0e\x1d:\xb4\xd8\xedv\x1f\xcd\
\xcc\xcc\xbcK\xd34\x14EAD\x0c\x86\xc7\x07\x98.\xf43\xdad\xe2\xcc\xb4cw\x1aI\
\x0bY\x99\xc8\xaa\xd0 \xa2\xd3\xf8\xb7\x1a\xb1a\x9d\x89?h,\xfb\xb6\xc6x\xad\
\x86g\xa5F\xc5\xa3\x1a\xaaP\x11q\x15U\xb1\n_$\xdb\xad\xa2(\xa8\x8b\xc2\xf4i\
\x17\xa1\xdd\x83SO#uo\x0b$C\x08\xf1\xe0\xe5\xcb\x97\xc3\xe5\xe5\xe5\xf3 \xe6\
e\xe0\xb5\xd7^\xbb\xd7\xe5r\xfd\x97\xc3\xe1H\x07\x90R\x12\x8dF\t\x04\x0246_\
\xe0\xbd\xea\xa7\x99v\x84\x98\xe9\x17d/\xcc"\xb7\xdc\x8d\x92\xaa\x85\xa4\xad\
\x92]i\xec\xbcJ\xf3^\x15\xefj\x95\xca\xbfS\x18x]!tNafP`\xcb\x01\xefzXp7 \xa4\
\x95\x05\x99@\x92`&d\xe2z\xfbf\xca\x8c[p\xbb\xdd)K%\x17?\x12\x89\xc4+\xa6i>|\
\xef\xbd\xf7\x86R\x198x\xf0\xa0\xfb\xca\x95+\xcf\xba\xdd\xee\x7f4\x0c\xc3\
\x96\xec\xcb\xa9\xd6f-<\x92\x89+0\xbe\xac\x19-M\x12\x1e\x99f\xba\x17\xb2\x16\
\xb8\xd1T=%^\x15\xd6j<\xd3\xa3\x11\x0fh\xac\xf8\xbeF\xd7>\x9d\x89\x06\x8d\
\xa5\x8f\xa8\x94>\xa8\xe2]\xad\xd1\xfbk\x15EUp\x97$\xd7\x0baE[\xb5+\xc4\x97\
\xf8\xf1\x0f\xf9\xc9\x1c+\xc6n\xb7\xa72\x91\x84Y\xa6i\xda\x97GFF\xc6\x0b\x0b\
\x0b\xeb\x14\x80\x17_|\xf1K>\x9f\xefa\x9b\xcd6\xbb2\x92H$R\xd7\xaa\xaa\xe2\
\xc9\xf2P\xa2\xad\xc1\xd9\xb8\x14\xc5\x06v\x1fDlc\\<\xdeCt<\x91\x12?[\x13\
\xbeu:k\x9e\xd4\x91S:c\xef\xe9,\xfd\x0b\x1d\xc3a\xd0\xfc}\x03\x196\xa8\xfc\
\xaeA\xfeV\xc3\xeaP\xe8\xc9Vku4M\xd316\rr\xa1\xec%fff\x10B\xa0(\n\x8a\xa2\
\xe0v\xbb\xa9\xae\xae^x\xfe\xfc\xf9\x7f\x15\xc9\xed\x02\x8d\x8d\x8d\x89\x1f\
\xfd\xe8G\xf8\xfd~\xd2\xd2\xd2\x88\xc7\xe3\xc4b1b\xb1X\n\xc60\x0c\xbc\x1e/\
\x8b\x9a\xefD\x99r\xa0\xe8`d\x82\x967M\xd3\xd9\x16F\xbb\xc3\xf3\xbb\x93\xd0\
\xd0\x14\x1dW\xb6N\xc1v\x9d\xfe\xff\xd6\x19\xd8\xaf\xa3k:JT\xc7\x95k][\xa2\
\xf5k\xba\x9b\xca\xcc0\xa8\xa7\x17[\xf5\xa1Zu\x92\x97\x97\xc7\x92%K\xd8\xbbw\
/\xaf\xbc\xf2\x8a\x04P\x84\x10BJ\xe9\x00x\xfe\xf9\xe7y\xfd\xf5\xd7\xc9\xce\
\xce\xc64M\xe2\xf18\xf1x\x1c\xd34\x11B\xe0v\xbb)\xc8."\xeb\xe4z\x84\x06B\x07\
-\rlEqZ\xba/\xd3V\xdf\x8b@\x9d\xb3FX\xebD\xe9\x83:%\x0f\xe8\xe4\xdcb\xb0\xf2\
\t\x83\xac\xf2d\xe4\xdf\'\xfa*\x1a\xa1Kq\xe4\x0b+Y\x96u\x0b\x0e\x87\x03UU\
\xa9\xa8\xa8 \x16\x8b\xf1\xe8\xa3\x8f\xd2\xd1\xd11\xbbn(*\xa0\xe6\xe7\xe7\
\x7f)\x18\x0c\xae\xcb\xcc\xcc\xc4\xef\xf7s\xe1\xc2\x056n\xdcH<\x1e\'\x1a\x8d\
\xa6\nHUU\x14\xa1\xc0\xb0\x8ba\xa3\x9dD\xce\x04B\x01E\x03-\x1d\xc23c\x0c\xb7\
\x8c\x93\x97\x9b\x87M\xb7\xa5\n\\C\xc7\x99\xad\xe3.\xd6P\x15\r5\xb5oR\x10BA\
\x11W\xd7\x87\x9e\x13\x93xj\xd6\xb3\xb2b5\xd9\xd9\xd9\xa4\xa7\xa7\xb3|\xf9r\
\xdex\xe3\r\x9e}\xf6YL\xd3dtt\x94\xe1\xe1a\x11\x08\x04\xfeA\x054\xaf\xd7\xbb\
\xb6\xb2\xb2r\xeb\xa5K\x97p\xb9\\D\xa3Qjjj\xa8\xac\xac\xc4\xe7\xf3155\x85\
\x10"UH\x003\xcdN\xc6\x965"\xf4\x04BX\xa1\xd0\x1c\x10\xb7M\xd3\xdd\xd2G\xa6#\
\x07\xb7+=\x99\x05-\x19\xe5\xab\x16\xbb\xba\xe1K\xee\x91\xcc\x04m/M\xb3\xb8o\
+\x95\xcbo\xc2\xeb\xf5\x92\x9f\x9fOaa!O>\xf9$\xef\xbe\xfb\xae\xb5 tw\xa3\xeb\
:yyy\xb1\xd6\xd6\xd6\x1f\xab\x80-\'\'\xe7\xce\xf4\xf4\xf4\rEEE\xf8\xfd~\xa2\
\xd1(\x0e\x87\x83\xba\xba:\x00\xaa\xaa\xaa\x08\x87\xc3V\xdfMB\xc8\x19\x95\
\xd1\xfe\tbK\xfd\x08\x01B\xb1\xa6j\x03%-No_\x17\x84\r|\xd9\xf9\xd7Y\xca\xeaX\
V\xe7A\x08"\x133\xb4\xfe\xbbI\x95\xfd\x0e\xca\xca\xca\xc8\xc8\xc8\xa0\xb4\
\xb4\x94H$\xc2\xee\xdd\xbb\x19\x1c\x1c$\x16\x8b\xd1\xda\xdaJEE\x05\x0e\x87\
\x83\xc9\xc9\xc9xgg\xe7OU\xc0\xaei\x9a\xc7\xedv\xdf#\x84\x10^\xaf\x17M\xd3\
\xe8\xed\xed%==\x9d\xee\xeen\x9a\x9b\x9b\xd9\xb2eK\xaa\x1ef;\x82\xd9\x95\xc6\
H\xcee\xf0L_\x85PA1,K\x8dL\xf73\xdc\x19ba\xee"\x0c\xd5\x96\xda\xfc\xa5\xec\
\x83`hh\x98\xbe\xe7\xd2\xa8.\xbe\x9dE\x8b\x16\xe1\xf1x(//\xe7\xf8\xf1\xe3<\
\xfd\xf4\xd3\xc4\xe3q\xc6\xc6\xc6\xe8\xeb\xebc\xed\xda\xb5\xa9\xcd_OOO[ \x10\
\xf8\x95\n8\xc2\xe1\xf0\xf0\xc0\xc0@\xa3\xddn\xbf\xc3\xe5r\xe9\xaa\xaaR\\\\\
\xcc\xa5K\x97p:\x9dD"\x11N\x9e<\xc9\xea\xd5\x96/#\x91H\xcaJ\x93\x17u&\xab\
\x9a\x11\x9a\x9c\x97\t\xa1\x81\xe6\x82\x88}\x8c\xb6\xd6\x0er\xd3\x16\x92f\
\xcfHu\x1c!\x14Zz\x9b\xf8}\xf05\xca\x1c\xb7\xb0\xcc\xbb\x96\x82\x82\x02\n\
\x0b\x0by\xea\xa9\xa78v\xec\x18\x00\xbd\xbd\xbd\xd8l6\xca\xca\xca\x98\x98\
\x98`ff\x86\x86\x86\x86w:::\x1e\x03\xc2*\xd6\x91\x89#\x1e\x8fG\xfb\xfb\xfb\
\xcf\xc6b\xb1U^\xaf73\x1a\x8dRRRBoo/\xb1X\x0c\xa7\xd3ICC\x03\x86ap\xd3M7155\
\x85\xa2(\x106\x08\x8c\r\x91(\x19\xba*\xder\x06\x8afY\x8a\xf4\x19\xda\xfd\
\xcd\xd8\xa2\xe9\xe4f\x14 \xa4\xc2\xa9\xe6\xb7\xb8(Ob_ \x19\xcfk\xe7\xcf\xf4\
\x870\x14;\xbbw\xef\xc6\xef\xf7\x13\x8b\xc5hoog\xc5\x8a\x15\xe8\xba\xce\xcc\
\xcc\x0c###\xe6\xd9\xb3g\x7f\x13\x0c\x06\x7f\r\x8c\x02\xe3*\xd6\xb1\x89\x06\
\x18\x80\x16\n\x85.\x04\x02\x814\x8f\xc7S\x9cH$DNN\x0e\x86a\xd0\xd7\xd7\xc7l\
\x97\xea\xec\xecd\xc3\x86\r\xa9\xe7[\xb3%\x9d\xd1\xc5M\xe0\x8e\xce\x87HZJ\
\xb5\x81\xe2N\xd0;\xd9J\xb0\x7f\x8c\xa6\x81s\x0cf6a\xcb\x01UQ\xf0\x8c\x97\
\x12nT\xd9\xf7//\xa4,300\xc0m\xb7\xdd\xc6\xe8\xe8(\x89D\x82\xb6\xb6\xb6\x89\
\x86\x86\x86\x9fG\xa3\xd1:`\x08\x08\xce\x02(\x90l\x05\xc9\x11\x89D\xba\xfb\
\xfb\xfb\x07\x1c\x0e\xc7M\x0e\x87CUU\x95\xb2\xb22\x1a\x1b\x1b\xc9\xc8\xc8 \
\x16\x8bQWWGUU\x15\x99\x99\x99\xccD\xa2\x8c7\xc3\xf4\xea\xcb\xd7\x03$\x9f8\
\x84\n\xaa\x13\xc2\xb6A\xa2\xe9!\xecC>\xbc\xe7n\xa5\xba\xf9\x9b\xf4\xfdF\xe7\
\x9d\xdf\xbe\x87"\x14\x86\x86\x86\xb0\xdb\xedTTT022B4\x1a\xa5\xbe\xbe\xfeJGG\
\xc7>)e\x17\xd0\x07\x0c&\x01\xc2\xb3G\x1a\x89\xe44\x818`\x9a\xa6\x19\xea\xef\
\xef\xbf`\x9af\x85\xcf\xe7K\x8bF\xa3TVV\xd2\xd6\xd6\x86\x94\x12\x97\xcbEss\
\xb3\xd51JJ\x99\xea\x95\x0c\xc5\xba\x91\xc5\xa3V-\xcc\x0b\x895\xb4\x1e/\xe9\
\xe7W\xb3\xf8\xdd{\xd90\xf3U*]\x1bx\xe5\xa5\x83\\\xe8\xbd@CS\x03\x13\x81\t6n\
\xdc\x88\xa6iLNN\x12\x0c\x06\x13\xef\xbd\xf7\xde\x1b\xc1`\xf0PR\xb8\x1f\xe8\
\x07\x02\xc080=\x0b \xe7\x00\xc4\xe6\xcc\x99\xd1\xd1\xd1\x86@ \xe0\xce\xcd\
\xcd](\xa5\xa4\xa8\xa8\x08\x80\x9e\x9e\x1e|>\x1f\x03\x03\x03\x04\x02\x01V\
\xae\\I\xb8\xde X^\x0fv3%Z\xf4e\x90vf%\x85\xc7vQ\xd1u7\x95\x8eMl^{;\xc1\x91 \
\xfb~\xb1\x8f\xba\xdc:\xba\x1e\xea\x82w\xe0\xb3\xeb>\xcb\xe4\xe4$\xa6i\xd2\
\xde\xde>YWW\xf7\x1f\xd1h\xf4\\R\xb8\x1f\x18\xc0:\xc2\x1c\x07\xa6\x81\xd8\\\
\x009\'\x03s!\xe2SSS\x1d\xdd\xdd\xdd\xc3\x19\x19\x19\x95.\x97Ku8\x1c\x94\x95\
\x95q\xe6\xcc\x19rrrH$\x12tttP^\xb2\x8cpw\x82)o?\x8e\xd3\xcb\xc9=z\'\x15-\
\xf7P\xae\xdcFIn\x05eK\xcbX\xb3z\r\xfb\xf7\xef\xe7\xc0\x81\x03\x9c\xf3\x9d#\
\xf4\x8b\x10\xb4\x81\xf3u\'UeULLL\xd0\xd0\xd0\xd0\xde\xd6\xd6\xf6\x9c\x94\
\xb2}\x8e\xf8\xc1\xa4\xf8pR|\x1c0\xc5\x9c\xd3\x80\xd9\x13h5Y\xd0N \x1d\xf0\
\x029@\x1eP\\UU\xf5\x97k\xd7\xae\xcd6\x0c\x83\x82\x82\x02\xde~\xfbm\x16,X\
\x80\xcf\xe7czz\x9a\xc2\x85\x85x<\x1e\xa2\xd1(\xe9\xeet\xd2\xd2\xd2p\xb9\\\
\x14\x17\x17\xa3\xaa*{\xf7\xee%\x14\nq\xa2\xe6\x04\xe3\xff<\x0e\x02\x8a\xde.\
\xe2s\xb9\x9f\xe3\xec\xe9\xb3\xb2\xbe\xbe\xfew\xe1p\xf8DR\xf0`\xb2`G\x80\t`\
\n\x88&\x03\x9d\x90R\xcay\x0f4I\x08\x91\x84\xd0\xb1Z\xac\x1b\xc8JB\xe4\x02\
\xb9\x0b\x17.\xfc\xe2\xed\xb7\xdf\xbe6++\x8b\xac\xac,zzz\x08\x04\x02,_\xbe\
\x1c\xd34\xf1z\xbdl\xd9\xb2\x85p8\x8c\xcdf\xa3\xb4\xb4\x94\xfa\xfaz\x9ey\xe6\
\x19\x00&\'\'9v\xe2\x18\x14\xc0\xf6\xbb\xb7\xb3\x88E\x9c>yz\xea\xe2\xc5\x8b/\
\x98\xa6\xd9\x92\xb4\xca 0\x8c\xd5.\xc3@$\xe9\x08\x13\x902)\xfc\xbag\xe29\
\x10\nV{\xb5\x01. \x13\xc8\x06|@\x9e\xdb\xed\xbeu\xfb\xf6\xed_,--U\xedv;.\
\x97\x8b\xa3G\x8fR]]\x8d\xcdfC\xd7u\xee\xb8\xe3\x0e\x16/^\xccs\xcf=\xc7\xa9S\
\xa7\x00\x18\x1e\x1e&33\x933g\xce\xf0\xf07\x1f\xa6\xb3\xb5\x93\xd3\xbf?\xdd\
\xe5\xf7\xfb\x7f\x99,\xd0\xd9\xc8\x07\x801`\x12\x98IZ&1W\xfc\xfb\x02\\\x03\
\xf2A\x96\xf2%\xb3\xb1h\xf3\xe6\xcd\x7f\xb5a\xc3\x86,\xc30\xc8\xcf\xcf\xe7\
\xd0\xa1C,Y\xb2\x84\x05\x0b\x16 \xa5\xe4\xca\x95+\x0c\x0f\x0fc\x9a&\xfd\xfd\
\xfdl\xd9\xb2\x85\xc1\xc1AJJJ\xa8\xa9\xa9\x91\xb5\xb5\xb5\xc7\'&&\x8e^c\x99 \
V\xa1^g\x99\xebt~\x84\xcf\x07>\xccR\xf9\xe5\xe5\xe5_\xd9\xb5k\xd7\xca\xac\
\xac,\xbc^/\x17.\\ \x1c\x0e\xb3j\xd5*\xce\x9f?\xcf\xc0\xc0\x00\xe3\xe3\xe3\
\xec\xda\xb5\x8b\xda\xdaZ\xa4\x94tuuE\x9a\x9a\x9a~i\x9a\xe6\xa5\xa4\xf0\x81d\
\xd4G\xb1\xfc\xfe\xbe\x96\xf9X\x00\xd7@|\x90\xa5r\x81\xdc\xcc\xcc\xcc\x8d\
\xf7\xddw\xdf\xfdK\x97.U\x1c\x0e\x07B\x08\x8e\x1c9\xc2\xe4\xe4$N\xa7\x93\xb2\
\xb22ZZZ\x88\xc7\xe3466\xf6\xf4\xf4\xf4\xbc\x80e\x99Y\xbf\x7f$\xcb|l\x80k@>\
\xccR\x8bw\xed\xda\xf5\xad\xad[\xb7\xba\r\xc3 77\x97\xc6\xc6F:;;\t\x85B\x8c\
\x8f\x8f\xcb\xfa\xfa\xfaw\xc6\xc6\xc6\x8ep5\xea\x1f\xcb2\x9f\x18`\x0e\xc4\
\x87Y*o\xe5\xca\x95\x0f\xdd\x7f\xff\xfd\x95YYY\x9c9s\x86P(DOO\xcfLSS\xd3\x8b\
\xb1X\xac\x89\xab~\x9f\xed2\x1f\xd927\x04p\r\xc4\x1f\xebR\xb9999\x9b\x1fx\
\xe0\x81\xfb\x86\x86\x86\x94S\xa7N\xf9\xbb\xba\xba\xfe\x93\xab]f\x08\xcb2!>\
\xa6en\x18\xe0\x1a\x90\xf7\xb3\x94\x07+\x1b\xd9\x9a\xa6\x15eddT\x8d\x8c\x8c\
\x9c\xc0\xf2w\x00+\xea\x9f\xd82\x9f\x1a\xc0\x1c\x88k-\x95\x86\x95\x8dL,{\xe9\
X\xd6\x98\xc0\x8ax\x88?\xb20}\xdcqC_5\x90R\xca\xe4\xd9\xa5\xc9\xfc\ra\x14\
\xcb\x1a\xf6\xe4=\xe2I\xc1\x93X\xfb\x98Ol\x99k\xc7\re`\xde?\x9ao)}\xceT\xb0\
\x84\xce\xdd ~b\xcb\\w\xdfO\xf3\xeb6\xd7\x14\xf8\xec\xb4>\x08\xb8\xfa\xccq\
\xc3Q\x9f;\xfe\x07\x11\x14b\x909%h\xa8\x00\x00\x00\x00IEND\xaeB`\x82`\x80b\
\x8f'
