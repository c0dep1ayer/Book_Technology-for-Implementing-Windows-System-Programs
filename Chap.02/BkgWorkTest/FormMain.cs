using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace BkgWorkTest
{
	public partial class FormMain : Form
	{
		ManualResetEvent _pause = new ManualResetEvent(true);

		public FormMain()
		{
			InitializeComponent();
		}

		private void _bkgWorker_DoWork(object sender, DoWorkEventArgs e)
		{
			int id = Thread.CurrentThread.ManagedThreadId;
			_bkgWorker.ReportProgress(0, (int)id);

			int delay = (int)_nudDelay.Value;
			for(int i=1; i<=100; i++)
			{
				if (_bkgWorker.CancellationPending)
				{
					e.Cancel = true;
					break;
				}

				Thread.Sleep(delay);
				_bkgWorker.ReportProgress(i);
				if (i == 100)
					break;
				_pause.WaitOne();
			}
			e.Result = "Completed!!!";
		}

		private void _bkgWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
		{
			int id = Thread.CurrentThread.ManagedThreadId;
			_prgBar.Value = e.ProgressPercentage;
			if (e.ProgressPercentage == 0)
				_tbLog.AppendText(string.Format("DoWork, Id = {0}{1}{1}", (int)e.UserState, Environment.NewLine));
			else
			{
				int len = _tbLog.Text.Length;
				int index = _tbLog.Text.LastIndexOf(Environment.NewLine, len - 2);
				if (index >= 0)
				{
					_tbLog.Select(index + Environment.NewLine.Length, len);
					_tbLog.SelectedText = string.Format("ProgressChanged => {0}%, Id = {1}{2}", 
						e.ProgressPercentage, id, Environment.NewLine);
				}
				else
					_tbLog.AppendText(string.Format("ProgressChanged, Id = {0}{1}", id, Environment.NewLine));
			}
		}

		private void _bkgWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
		{
			int id = Thread.CurrentThread.ManagedThreadId;

			if(e.Cancelled)
				_tbLog.AppendText(string.Format("RunWorkerCanceled, Id = {0}{1}", id, Environment.NewLine));
			else
				_tbLog.AppendText(string.Format("RunWorkerCompleted, Id = {0}{1}", id, Environment.NewLine));

			_btnStart.Enabled = true;
			_btnPause.Enabled = false;
			_btnCancel.Enabled = false;
		}

		private void _ButtonClick_Start(object sender, EventArgs e)
		{
			int id = Thread.CurrentThread.ManagedThreadId;
			_tbLog.Text = string.Format("RunWorkerAsync, Id = {0}{1}", id, Environment.NewLine);
			_bkgWorker.RunWorkerAsync(this);

			_btnStart.Enabled = false;
			_btnPause.Enabled = true;
			_btnCancel.Enabled = true;
		}

		private void _ButtonClick_Pause(object sender, EventArgs e)
		{
			if (!_bkgWorker.IsBusy)
				return;

			int id = Thread.CurrentThread.ManagedThreadId;
			if (_pause.WaitOne(0))
			{
				_btnPause.Text = "Restart";
				_pause.Reset();
				_tbLog.AppendText(string.Format(" ** Paused, Id = {0}{1}{1}", id, Environment.NewLine));
			}
			else
			{
				_btnPause.Text = "Pause";
				_pause.Set();
				_tbLog.AppendText(string.Format(" ** Resumed, Id = {0}{1}{1}", id, Environment.NewLine));
			}
		}

		private void _ButtonClick_Cancel(object sender, EventArgs e)
		{
			if (!_bkgWorker.IsBusy)
				return;

			if (_pause.WaitOne(0))
				_pause.Reset();

			DialogResult dr = MessageBox.Show("현재작업을 취소하시겠습니까?", 
				"BkgWorkTest", MessageBoxButtons.YesNo);
			if (dr == DialogResult.Yes)
				_bkgWorker.CancelAsync();
			else
			{
				if (e is FormClosingEventArgs)
					((FormClosingEventArgs)e).Cancel = true;
			}
			_pause.Set();
		}

		private void FormMain_FormClosing(object sender, FormClosingEventArgs e)
		{
			_ButtonClick_Cancel(sender, e);
		}
	}
}
