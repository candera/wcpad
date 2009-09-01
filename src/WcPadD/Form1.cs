using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Threading;

namespace Wangdera.WcPad.Driver
{
    public partial class Form1 : Form
    {
        private readonly Thread _thread; 
        public Form1()
        {
            _thread = new Thread(Run);
            _thread.IsBackground = true; 
            InitializeComponent();
        }

        private void Run(object data)
        {
            WcPadInterop.Initialize();
            try
            {
                while (true)
                {
                    Thread.Sleep(100);
                    int n = WcPadInterop.Update();
                    FingertipInfo[] fingertips = null;

                    if (n > 0)
                    {
                        fingertips = new FingertipInfo[n];
                        for (int i = 0; i < n; ++i)
                        {
                            fingertips[i] = WcPadInterop.GetFingertipInfo(i);
                        }
                    }

                    Action uiUpdate = () =>
                    {
                        Bitmap bitmap = pictureBox1.Image as Bitmap;
                        if (bitmap == null)
                        {
                            bitmap = new Bitmap(pictureBox1.Width, pictureBox1.Height);
                            pictureBox1.Image = bitmap;
                        }

                        Graphics g = Graphics.FromImage(bitmap);

                        g.Clear(Color.Black);

                        if (n > 0)
                        {
                            for (int i = 0; i < n; ++i)
                            {
                                g.DrawString(
                                    i.ToString(),
                                    new Font(FontFamily.GenericMonospace, 8),
                                    Brushes.White,
                                    new PointF(fingertips[i].x * bitmap.Width, fingertips[i].y * bitmap.Height));
                            }
                        }
                        else
                        {
                            g.Clear(Color.Red);
                        }
                        pictureBox1.Invalidate(); 
                    };

                    BeginInvoke(uiUpdate);
                }
            }
            finally
            {
                WcPadInterop.Cleanup(); 
            }
        }

        private void bStart_Click(object sender, EventArgs e)
        {
            _thread.Start(); 
        }

    }
}
