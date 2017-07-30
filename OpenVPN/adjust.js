// Expand a variable in a config.
// Run multiple times to expand multiple variables.

var forReading = 1;
var forWriting = 2;

var args = WScript.Arguments;
var fileSrcPath = args.Item (0);
var variable = args.Item (1);
var value = args.Item (2);

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
