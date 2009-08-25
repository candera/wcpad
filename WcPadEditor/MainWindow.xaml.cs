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

namespace Wangdera.WcPadEditor
{
    public partial class MainWindow : Window
    {
        private readonly PadRegionsViewModel _model = new PadRegionsViewModel();

        public MainWindow()
        {
            InitializeComponent();

            this.CommandBindings.Add(new CommandBinding(ApplicationCommands.Print, Print)); 
            this.CommandBindings.Add(new CommandBinding(AppCommands.Exit, Exit));
//            this.CommandBindings.Add(new CommandBinding(AppCommands.SetupPrinter, SetupPrinter)); 

            this.DataContext = _model;

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
            _model.MouseUp(e); 
        }

        void MainWindow_MouseMove(object sender, MouseEventArgs e)
        {
            // TODO: Do something more efficient than FindItemsPanel
            _model.MouseMove(e.GetPosition(FindItemsPanel<Panel>(sender as Visual))); 
        }

        void MainWindow_MouseDown(object sender, MouseButtonEventArgs e)
        {
            // TODO: Do something more efficient than FindItemsPanel
            _model.MouseDown(e, e.GetPosition(FindItemsPanel<Panel>(sender as Visual))); 
        }

        void Exit(object sender, ExecutedRoutedEventArgs e)
        {
            Close(); 
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
