using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Windows.Xps;
using System.Printing;
using Microsoft.Win32;
using System.IO;

namespace Wangdera.WcPadEditor
{
    public partial class MainWindow : Window
    {
        private PadRegionsViewModel _model;
        private OpenFileDialog _openFileDialog = new OpenFileDialog
            {
                DefaultExt = ".wcpad",
                Filter = "wcpad files (.wcpad)|*.wcpad|All Files (*.*)|*.*"
            }; 
        private SaveFileDialog _saveFileDialog = new SaveFileDialog
            {
                AddExtension = true,
                DefaultExt = ".wcpad",
                Filter = "wcpad files (.wcpad)|*.wcpad|All Files (*.*)|*.*"
            };

        public MainWindow()
        {
            InitializeComponent();

            this.CommandBindings.Add(new CommandBinding(ApplicationCommands.New, FileNew));
            this.CommandBindings.Add(new CommandBinding(ApplicationCommands.Open, FileOpen));
            this.CommandBindings.Add(new CommandBinding(ApplicationCommands.Save, FileSave, CanSaveFile));
            this.CommandBindings.Add(new CommandBinding(ApplicationCommands.SaveAs, FileSaveAs, CanSaveFile));
            this.CommandBindings.Add(new CommandBinding(ApplicationCommands.Close, FileClose, CanCloseFile));
            this.CommandBindings.Add(new CommandBinding(ApplicationCommands.Print, Print, CanPrint)); 
            this.CommandBindings.Add(new CommandBinding(AppCommands.Exit, Exit));
//            this.CommandBindings.Add(new CommandBinding(AppCommands.SetupPrinter, SetupPrinter)); 

            Border.MouseDown += new MouseButtonEventHandler(MainWindow_MouseDown);
            Border.MouseMove += new MouseEventHandler(MainWindow_MouseMove);
            Border.MouseUp += new MouseButtonEventHandler(MainWindow_MouseUp);
        }

        private T FindItemsPanel<T>(Visual visual)
        {
            for (int i = 0; i < VisualTreeHelper.GetChildrenCount(visual); i++)
            {
                Visual child = VisualTreeHelper.GetChild(visual, i) as Visual;

                if (child != null)
                {
                    if (child is T && VisualTreeHelper.GetParent(child) is ItemsPresenter)
                    {
                        object temp = child;
                        return (T)temp;
                    }

                    T panel = FindItemsPanel<T>(child);

                    if (panel != null)
                    {
                        object temp = panel;
                        return (T)temp; // return the panel up the call stack
                    }
                }
            }

            return default(T);
        }

        void MainWindow_MouseUp(object sender, MouseButtonEventArgs e)
        {
            if (_model != null)
            {
                _model.MouseUp(e);
            }
        }

        void MainWindow_MouseMove(object sender, MouseEventArgs e)
        {
            if (_model != null)
            {
                // TODO: Do something more efficient than FindItemsPanel
                _model.MouseMove(e.GetPosition(FindItemsPanel<Panel>(sender as Visual)));
            }
        }

        void MainWindow_MouseDown(object sender, MouseButtonEventArgs e)
        {
            if (_model != null)
            {
                // TODO: Do something more efficient than FindItemsPanel
                _model.MouseDown(e, e.GetPosition(FindItemsPanel<Panel>(sender as Visual)));
            }
        }

        void Exit(object sender, ExecutedRoutedEventArgs e)
        {
            Close(); 
        }

        private void CanSaveFile(object sender, CanExecuteRoutedEventArgs e)
        {
            e.CanExecute = this._model != null; 
        }
        private void CanPrint(object sender, CanExecuteRoutedEventArgs e)
        {
            e.CanExecute = this._model != null; 
        }
        private void CanCloseFile(object seender, CanExecuteRoutedEventArgs e)
        {
            e.CanExecute = this._model != null; 
        }

        private void FileNew(object sender, ExecutedRoutedEventArgs e)
        {
            this._model = new PadRegionsViewModel(); 
            this.DataContext = _model;
        }
        private void FileOpen(object sender, ExecutedRoutedEventArgs e)
        {
            // TODO: Detect existing model and prompt for save
            if (_openFileDialog.ShowDialog() == true)
            {
                using (Stream stream = File.Open(_openFileDialog.FileName, FileMode.Open, FileAccess.Read, FileShare.Read))
                {
                    _model = PadRegionsViewModel.Load(stream);
                    DataContext = _model; 
                }
            }
            
        }
        private void FileSave(object sender, ExecutedRoutedEventArgs e)
        {
            if (_saveFileDialog.ShowDialog() == true)
            {
                using (Stream stream = File.Open(_saveFileDialog.FileName, FileMode.Create, FileAccess.Write, FileShare.None))
                {
                    _model.Save(stream);
                }
            }
        }
        private void FileSaveAs(object sender, ExecutedRoutedEventArgs e)
        {
            MessageBox.Show("Not yet implemented.");
        }
        private void FileClose(object sender, ExecutedRoutedEventArgs e)
        {
            // TODO: Check for save required
            this._model = null; 
            this.DataContext = null; 
        }

        private void Print(object sender, ExecutedRoutedEventArgs e)
        {
            PrintDocumentImageableArea area = null; 
            XpsDocumentWriter dw = PrintQueue.CreateXpsDocumentWriter(ref area);

            if (dw != null)
            {
                Size outputSize = new Size(area.MediaSizeWidth, area.MediaSizeHeight);
                Paper.Measure(outputSize);
                Paper.Arrange(new Rect(outputSize));
                Paper.UpdateLayout();

                dw.Write(Paper);
            }
        }

        //private void SetupPrinter(object sender, ExecutedRoutedEventArgs e)
        //{
        //    System.Windows.Forms.PageSetupDialog dialog = new System.Windows.Forms.PageSetupDialog();
        //    dialog.PageSettings = new System.Drawing.Printing.PageSettings(); 
        //    dialog.ShowDialog(); 
        //}
    }
}
