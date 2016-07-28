// Expand variable in OpenVPN config.
// Run multiple times to expand several variables

var forReading = 1;
var forWriting = 2;

var args = WScript.Arguments;
var fileSrcPath = args.item (0);
var variable = args.item (1);
var value = args.item (2);

var fso = WScript.CreateObject ("Scripting.FileSystemObject");
var fileSrc = fso.OpenTextFile (fileSrcPath, forReading);
var fileNew = fso.CreateTextFile (fileSrcPath + ".tmp");

do
{
  fileNew.WriteLine (fileSrc.ReadLine().replace (variable, value));
}
while (!fileSrc.AtEndOfStream);

fileSrc.Close();
fileNew.Close();

fileSrc = fso.GetFile (fileSrcPath);
fileSrc.Delete (true);

fileNew = fso.GetFile (fileSrcPath + ".tmp");
fileNew.Move (fileSrcPath);
