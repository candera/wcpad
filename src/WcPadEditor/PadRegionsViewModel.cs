using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Collections.ObjectModel;
using System.Windows.Input;
using System.ComponentModel;
using System.Windows;
using System.IO;
using System.Xml.Linq;
using System.Xml;

namespace Wangdera.WcPadEditor
{
    internal class PadRegionsViewModel : ObservableCollection<PadRegion>, INotifyPropertyChanged
    {
        private int _borderMargin = 96 / 2;
        private int _borderPadding = 96 / 2;
        private int _borderThickness = 96 / 3; 
        private bool _isLeftButtonDown;
        private Point _mouseDownPosition; 
        private Point _mousePosition;
        private int _pageHeight = 11 * 96;  // 11 inches
        private int _pageWidth = 816;       // 8.5 inches

        public int BorderMargin
        {
            get
            {
                return _borderMargin;
            }
            set
            {
                if (_borderMargin != value)
                {
                    _borderMargin = value;
                    OnPropertyChanged("BorderMargin");
                }
            }
        }
        public int BorderPadding
        {
            get
            {
                return _borderPadding;
            }
            set
            {
                if (_borderPadding != value)
                {
                    _borderPadding = value;
                    OnPropertyChanged("BorderPadding");
                }
            }
        }
        public int BorderThickness
        {
            get
            {
                return _borderThickness;
            }
            set
            {
                if (_borderThickness != value)
                {
                    _borderThickness = value;
                    OnPropertyChanged("BorderThickness");
                }
            }
        }
        public bool IsLeftButtonDown
        {
            get
            {
                return _isLeftButtonDown;
            }
            set
            {
                if (_isLeftButtonDown != value)
                {
                    _isLeftButtonDown = value;
                    OnPropertyChanged("IsLeftButtonDown");
                }
            }
        }
        public Point MouseDownPosition
        {
            get
            {
                return _mouseDownPosition;
            }
            set
            {
                if (_mouseDownPosition != value)
                {
                    _mouseDownPosition = value;
                    OnPropertyChanged("MouseDownPosition");
                }
            }
        }
        public Point MousePosition
        {
            get
            {
                return _mousePosition;
            }
            set
            {
                if (_mousePosition != value)
                {
                    _mousePosition = value;
                    OnPropertyChanged("MousePosition");
                }
            }
        }
        public int PageHeight
        {
            get
            {
                return _pageHeight;
            }
            set
            {
                if (_pageHeight != value)
                {
                    _pageHeight = value;
                    OnPropertyChanged("PageHeight");
                }
            }
        }
        public int PageWidth
        {
            get
            {
                return _pageWidth;
            }
            set
            {
                if (_pageWidth != value)
                {
                    _pageWidth = value;
                    OnPropertyChanged("PageWidth");
                }
            }
        }


        internal void MouseDown(MouseButtonEventArgs e, Point position)
        {
            if (e.ChangedButton == MouseButton.Left)
            {
                IsLeftButtonDown = true;
                MouseDownPosition = position; 
            }
        }
        internal void MouseMove(Point position)
        {
            MousePosition = position;

            if (IsLeftButtonDown)
            {
                PadRegion active = this.FirstOrDefault(r => r.IsBeingModified);

                if (active == null)
                {
                    active = new PadRegion
                    {
                        IsBeingModified = true
                    }; 

                    Add(active);
                }

                active.X = (int)Math.Min(MouseDownPosition.X, position.X);
                active.Y = (int)Math.Min(MouseDownPosition.Y, position.Y);
                active.Width = (int)Math.Abs(MouseDownPosition.X - position.X);
                active.Height = (int)Math.Abs(MouseDownPosition.Y - position.Y);
            }
        }
        internal void MouseUp(MouseButtonEventArgs e)
        {
            if (e.ChangedButton == MouseButton.Left)
            {
                IsLeftButtonDown = false;
                PadRegion active = this.FirstOrDefault(r => r.IsBeingModified);
                if (active != null)
                {
                    active.IsBeingModified = false; 
                }
            }
        }
        internal void Save(Stream stream)
        {
            XDocument doc = new XDocument();
            doc.Add(new XElement("map",
                new XAttribute("version", "1"),
                new XElement("border",
                    new XAttribute("margin", BorderMargin),
                    new XAttribute("padding", BorderPadding),
                    new XAttribute("thickness", BorderThickness)),
                new XElement("page",
                    new XAttribute("height", PageHeight),
                    new XAttribute("width", PageWidth)),
                new XElement("regions",
                    from region in this
                    select
                        new XElement("region",
                        new XAttribute("x", region.X),
                        new XAttribute("y", region.Y),
                        new XAttribute("width", region.Width),
                        new XAttribute("height", region.Height)))));

            using (XmlWriter writer = XmlWriter.Create(stream, new XmlWriterSettings { IndentChars = "  ", Indent = true }))
            {
                doc.Save(writer);
            }       
        }

        private void OnPropertyChanged(string propertyName)
        {
            base.OnPropertyChanged(new PropertyChangedEventArgs(propertyName));
        }


    }
}
