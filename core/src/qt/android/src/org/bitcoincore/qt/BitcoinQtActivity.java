package org.bitcoincore.qt;

import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;

import org.qtproject.qt5.android.bindings.QtActivity;

import java.io.File;

public class BitcoinQtActivity extends QtActivity
{
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        final File rigorDir = new File(getFilesDir().getAbsolutePath() + "/.rigor");
        if (!rigorDir.exists()) {
            rigorDir.mkdir();
        }

        super.onCreate(savedInstanceState);
    }
}
