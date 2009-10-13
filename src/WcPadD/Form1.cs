using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Threading;
using Wangdera.WcPad.Common;
using System.IO;

namespace Wangdera.WcPad.Driver
{
    public partial class Form1 : Form
    {
        private PadRegionsViewModel _model;
        private readonly OpenFileDialog _openFileDialog = new OpenFileDialog
        {
            DefaultExt = ".wcpad",
            Filter = "wcpad files (.wcpad)|*.wcpad|All Files (*.*)|*.*"
        };
        private readonly Thread _thread;

        public Form1()
        {
            _thread = new Thread(Run);
            _thread.IsBackground = true;
            InitializeComponent();
        }

        private void Run(object data)
        {
        }

        private void UpdateUI()
        {
            int n = WcPadInterop.Update();
            FingertipInfo[] fingertips = null;

            if (n > 0)
            {
                toolStripStatusLabel1.Text = n.ToString() + " fingertips detected."; 
                fingertips = new FingertipInfo[n];
                for (int i = 0; i < n; ++i)
                {
                    fingertips[i] = WcPadInterop.GetFingertipInfo(i);
                }
            }
            else if (n == 0)
            {
                toolStripStatusLabel1.Text = "Map detected.";
            }
            else
            {
                toolStripStatusLabel1.Text = "No map detected."; 
            }

            Bitmap bitmap = pictureBox1.Image as Bitmap;
            if (bitmap == null)
            {
                bitmap = new Bitmap(pictureBox1.Width, pictureBox1.Height);
                pictureBox1.Image = bitmap;
            }

            Graphics g = Graphics.FromImage(bitmap);

            if (n < 0)
            {
                g.Clear(Color.Red);
            }
            else
            {
                g.Clear(Color.Black);
                DrawModel(g, fingertips);
            }

            pictureBox1.Invalidate();

        }

        private void DrawModel(Graphics g, FingertipInfo[] fingertips)
        {
            if (_model == null)
            {
                return;
            }

            float imageWidth = g.VisibleClipBounds.Width;
            float imageHeight = g.VisibleClipBounds.Height; 

            float pageAspect = (float)_model.PageWidth / (float)_model.PageHeight;
            float imageAspect = imageWidth / imageHeight;

            bool landscape = pageAspect > imageAspect;

            int borderWidth = _model.PageWidth - _model.BorderMargin;
            int borderHeight = _model.PageHeight - _model.BorderMargin;

            float ratio;
            if (landscape)
            {
                ratio = imageWidth / borderWidth;
            }
            else
            {
                ratio = imageHeight / borderHeight;
            }

//            ratio = 0.1F; 

            g.ScaleTransform(ratio, ratio);

            g.FillRectangle(Brushes.Black, 0, 0, borderWidth, borderHeight);
            g.FillRectangle(Brushes.White, _model.BorderMargin, _model.BorderMargin, borderWidth - _model.BorderMargin, borderHeight - _model.BorderMargin);

            int offset = _model.BorderPadding + _model.BorderThickness; 
            foreach (var region in _model)
            {
                g.DrawRectangle(Pens.Red, region.X + offset, region.Y + offset, region.Width, region.Height); 
            }

            int fingertipSize = 25; 

            if (fingertips != null)
            {
                foreach (var fingertip in fingertips)
                {
                    g.FillRectangle(
                        Brushes.Blue, 
                        (fingertip.x * borderWidth) - fingertipSize, 
                        (fingertip.y * borderHeight) - fingertipSize, 
                        fingertipSize * 2, 
                        fingertipSize * 2);
                }
            }
        }

        private void bStart_Click(object sender, EventArgs e)
        {
            //_thread.Start(); 

            WcPadInterop.Initialize();
            timer1.Enabled = true;
            
        }

        private void bTest_Click(object sender, EventArgs e)
        {
            WcPadInterop.Test();
        }

        private void bLoad_Click(object sender, EventArgs e)
        {
            if (_openFileDialog.ShowDialog() == DialogResult.OK)
            {
                using (Stream stream = new FileStream(_openFileDialog.FileName, FileMode.Open, FileAccess.Read, FileShare.Read))
                {
                    _model = PadRegionsViewModel.Load(stream);
                }
            }
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            UpdateUI(); 
        }

    }
}
