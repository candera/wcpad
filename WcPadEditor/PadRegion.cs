using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ComponentModel;

namespace Wangdera.WcPadEditor
{
    internal class PadRegion : INotifyPropertyChanged
    {
        private bool _isBeingModified; 
        private int _x;
        private int _y;
        private int _width;
        private int _height;

        public bool IsBeingModified
        {
            get { return _isBeingModified; }
            set
            {
                if (_isBeingModified != value)
                {
                    _isBeingModified = value;
                    OnPropertyChanged("IsBeingModfied");
                }
            }
        }
        public int X 
        {
            get { return _x; }
            set
            {
                if (_x != value)
                {
                    _x = value;
                    OnPropertyChanged("X"); 
                }
            }
        }
        public int Y
        {
            get { return _y; }
            set
            {
                if (_y != value)
                {
                    _y = value;
                    OnPropertyChanged("Y");
                }
            }
        }
        public int Width
        {
            get { return _width; }
            set
            {
                if (_width != value)
                {
                    _width = value;
                    OnPropertyChanged("Width");
                }
            }
        }
        public int Height
        {
            get { return _height; }
            set
            {
                if (_height != value)
                {
                    _height = value;
                    OnPropertyChanged("Height");
                }
            }
        }

        #region INotifyPropertyChanged Members

        public event PropertyChangedEventHandler PropertyChanged;

        private void OnPropertyChanged(string propertyName)
        {
            if (PropertyChanged != null)
            {
                PropertyChanged(this,
                                new PropertyChangedEventArgs(propertyName));
            }
        }

        #endregion
    }
}
