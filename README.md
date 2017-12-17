FastHeading.cpp : Defines the entry point for the console application.
Written by Douwe Fokkema december 2017.
 A simple apolication converting NMEA2000 Fastheading PGN127250 to
 NMEA183 HDM messages.
 The NGT-1 should be connected to COM port 7
 The NMEA183 output is at COM port 20.
 Both COM ports operate at 115000 Bps.
 Based on Håkan Svensson's SEND_NMEA_COM, see https://github.com/Hakansv/Send_NMEA_COM
 and
 Kees Verruyt's Canboat, see https://github.com/canboat/canboat/tree/master/actisense-serial

 This software is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License.
 If not, see <http://www.gnu.org/licenses/>.
