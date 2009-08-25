using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Input;

namespace Wangdera.WcPadEditor
{
    public static class AppCommands
    {
        private static readonly RoutedCommand _exit = new RoutedCommand();
        private static readonly RoutedCommand _setupPrinter = new RoutedCommand(); 

        public static RoutedCommand Exit
        {
            get { return _exit; }
        }

        public static RoutedCommand SetupPrinter
        {
            get { return _setupPrinter; }
        }
    }
}
