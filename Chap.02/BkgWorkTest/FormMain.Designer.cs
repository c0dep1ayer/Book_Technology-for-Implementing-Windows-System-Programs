namespace BkgWorkTest
{
	partial class FormMain
	{
		/// <summary>
		/// 필수 디자이너 변수입니다.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// 사용 중인 모든 리소스를 정리합니다.
		/// </summary>
		/// <param name="disposing">관리되는 리소스를 삭제해야 하면 true이고, 그렇지 않으면 false입니다.</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Windows Form 디자이너에서 생성한 코드

		/// <summary>
		/// 디자이너 지원에 필요한 메서드입니다.
		/// 이 메서드의 내용을 코드 편집기로 수정하지 마십시오.
		/// </summary>
		private void InitializeComponent()
		{
			this._prgBar = new System.Windows.Forms.ProgressBar();
			this._bkgWorker = new System.ComponentModel.BackgroundWorker();
			this._nudDelay = new System.Windows.Forms.NumericUpDown();
			this._btnStart = new System.Windows.Forms.Button();
			this._tbLog = new System.Windows.Forms.TextBox();
			this._btnPause = new System.Windows.Forms.Button();
			this._btnCancel = new System.Windows.Forms.Button();
			this.label1 = new System.Windows.Forms.Label();
			this.label2 = new System.Windows.Forms.Label();
			((System.ComponentModel.ISupportInitialize)(this._nudDelay)).BeginInit();
			this.SuspendLayout();
			// 
			// _prgBar
			// 
			this._prgBar.Location = new System.Drawing.Point(12, 12);
			this._prgBar.Name = "_prgBar";
			this._prgBar.Size = new System.Drawing.Size(259, 21);
			this._prgBar.Step = 1;
			this._prgBar.Style = System.Windows.Forms.ProgressBarStyle.Continuous;
			this._prgBar.TabIndex = 0;
			// 
			// _bkgWorker
			// 
			this._bkgWorker.WorkerReportsProgress = true;
			this._bkgWorker.WorkerSupportsCancellation = true;
			this._bkgWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this._bkgWorker_DoWork);
			this._bkgWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this._bkgWorker_ProgressChanged);
			this._bkgWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this._bkgWorker_RunWorkerCompleted);
			// 
			// _nudDelay
			// 
			this._nudDelay.Increment = new decimal(new int[] {
            10,
            0,
            0,
            0});
			this._nudDelay.Location = new System.Drawing.Point(333, 12);
			this._nudDelay.Maximum = new decimal(new int[] {
            2000,
            0,
            0,
            0});
			this._nudDelay.Minimum = new decimal(new int[] {
            10,
            0,
            0,
            0});
			this._nudDelay.Name = "_nudDelay";
			this._nudDelay.Size = new System.Drawing.Size(52, 21);
			this._nudDelay.TabIndex = 1;
			this._nudDelay.TextAlign = System.Windows.Forms.HorizontalAlignment.Right;
			this._nudDelay.Value = new decimal(new int[] {
            200,
            0,
            0,
            0});
			// 
			// _btnStart
			// 
			this._btnStart.Location = new System.Drawing.Point(333, 41);
			this._btnStart.Name = "_btnStart";
			this._btnStart.Size = new System.Drawing.Size(75, 23);
			this._btnStart.TabIndex = 2;
			this._btnStart.Text = "Start";
			this._btnStart.UseVisualStyleBackColor = true;
			this._btnStart.Click += new System.EventHandler(this._ButtonClick_Start);
			// 
			// _tbLog
			// 
			this._tbLog.Location = new System.Drawing.Point(12, 41);
			this._tbLog.Multiline = true;
			this._tbLog.Name = "_tbLog";
			this._tbLog.ReadOnly = true;
			this._tbLog.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
			this._tbLog.Size = new System.Drawing.Size(315, 81);
			this._tbLog.TabIndex = 3;
			// 
			// _btnPause
			// 
			this._btnPause.Enabled = false;
			this._btnPause.Location = new System.Drawing.Point(333, 70);
			this._btnPause.Name = "_btnPause";
			this._btnPause.Size = new System.Drawing.Size(75, 23);
			this._btnPause.TabIndex = 2;
			this._btnPause.Text = "Pause";
			this._btnPause.UseVisualStyleBackColor = true;
			this._btnPause.Click += new System.EventHandler(this._ButtonClick_Pause);
			// 
			// _btnCancel
			// 
			this._btnCancel.Enabled = false;
			this._btnCancel.Location = new System.Drawing.Point(333, 99);
			this._btnCancel.Name = "_btnCancel";
			this._btnCancel.Size = new System.Drawing.Size(75, 23);
			this._btnCancel.TabIndex = 2;
			this._btnCancel.Text = "Cancel";
			this._btnCancel.UseVisualStyleBackColor = true;
			this._btnCancel.Click += new System.EventHandler(this._ButtonClick_Cancel);
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(391, 16);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(23, 12);
			this.label1.TabIndex = 4;
			this.label1.Text = "ms";
			// 
			// label2
			// 
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(286, 16);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(49, 12);
			this.label2.TabIndex = 4;
			this.label2.Text = "Delay : ";
			// 
			// FormMain
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 12F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(418, 131);
			this.Controls.Add(this._nudDelay);
			this.Controls.Add(this.label2);
			this.Controls.Add(this.label1);
			this.Controls.Add(this._tbLog);
			this.Controls.Add(this._btnCancel);
			this.Controls.Add(this._btnPause);
			this.Controls.Add(this._btnStart);
			this.Controls.Add(this._prgBar);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.Fixed3D;
			this.Name = "FormMain";
			this.Text = "BackgroundWorker Test";
			this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.FormMain_FormClosing);
			((System.ComponentModel.ISupportInitialize)(this._nudDelay)).EndInit();
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.ProgressBar _prgBar;
		private System.ComponentModel.BackgroundWorker _bkgWorker;
		private System.Windows.Forms.NumericUpDown _nudDelay;
		private System.Windows.Forms.Button _btnStart;
		private System.Windows.Forms.TextBox _tbLog;
		private System.Windows.Forms.Button _btnPause;
		private System.Windows.Forms.Button _btnCancel;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label label2;
	}
}

