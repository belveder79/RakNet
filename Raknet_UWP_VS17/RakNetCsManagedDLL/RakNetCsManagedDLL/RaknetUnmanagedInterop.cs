using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

using System.Windows;

namespace RakNetCsManagedDLL
{
    public class RaknetUnmanagedInterop { 
    
        public delegate int Callback(string text);
        private Callback mInstance;   // Ensure it doesn't get garbage collected

        Callback textBoxDelegate = null;

        public RaknetUnmanagedInterop()
        {
            mInstance = new Callback(Handler);
        }
        public void Test(Callback xy)
        {
            textBoxDelegate = xy;
            Windows.Storage.StorageFolder storageFolder = 
                Windows.Storage.ApplicationData.Current.LocalFolder;
            int retval = InitializeRaknetLibrary(storageFolder.Path.ToString(), mInstance);
        }

        private int Handler(string text)
        {
            textBoxDelegate(text);
            return 0;
        }
#if DEBUG
        [DllImport("Raknetd.dll")]
#else
        [DllImport("Raknet.dll")]
#endif
        private static extern int InitializeRaknetLibrary(string foldername, Callback fn);

//        [DllImport("cpptemp1.dll")]
//        private static extern void TestCallback();
    }
}
