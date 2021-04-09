package com.i_syst.blystscale

import android.Manifest
import android.app.Activity
import android.bluetooth.*
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.os.Bundle
import android.util.Log
import android.view.View
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.*
import kotlin.math.absoluteValue

class MainActivity : Activity() {
    val BLEADV_MANDATA_TYPE_ADC: Byte = 7
    val BLEADV_MANDATA_SN = 0xFF.toByte()
    val AVERAGE_COUNT = 20

    var mBluetoothAdapter: BluetoothAdapter? = null
    var mLEScanner: BluetoothLeScanner? = null
    private var mAdcLabel: TextView? = null
    private var mWeightLabel : TextView? = null
    private var mZeroLabel : TextView? = null
    private var mScaleLabel : TextView? = null
    private var mMessageLabel : TextView? = null
    var zeroWeight = 0
    private var mZeroBut : Button? = null
    var mAvrStart : Boolean = false
    var mZeroVal : Float = 0.0F
    var mAvrCnt = 0
    private var mCalBut : Button? = null
    var mCalState = 0
    var mAvrVal = 0F
    var mScaleFactor = 20F
    var mWeight = 0F

    //private var mAdapter: com.i_syst.blystscale.MainActivity.DeviceListAdapter? = null

    private val mGattCallback: BluetoothGattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(
            gatt: BluetoothGatt, status: Int,
            newState: Int
        ) {
            var intentAction: String
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                gatt.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
            }
        }
    }


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        requestPermissions(
            arrayOf(
                Manifest.permission.ACCESS_COARSE_LOCATION,
                Manifest.permission.ACCESS_FINE_LOCATION
            ), 1
        )
        val bluetoothManager =
            getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager

        //mTempLabel = findViewById<View>(R.id.tempLabel) as TextView
        mMessageLabel = findViewById<View>(R.id.messageLabel) as TextView
        mZeroLabel = findViewById<View>(R.id.zeroLabel) as TextView
        mScaleLabel = findViewById<View>(R.id.scaleLabel) as TextView
        mAdcLabel = findViewById<View>(R.id.adcLabel) as TextView
        mWeightLabel = findViewById<View>(R.id.weightLabel) as TextView
        mBluetoothAdapter = bluetoothManager.adapter
        mLEScanner = mBluetoothAdapter?.getBluetoothLeScanner()
        mLEScanner?.startScan(mScanCallback)
        mZeroBut = findViewById<android.view.View?>(R.id.buttonZero) as android.widget.Button?
        mZeroBut?.setOnClickListener(
            object : View.OnClickListener {
                override fun onClick(v: View) { // Code here executes on main thread after user presses button
                    mAvrCnt = AVERAGE_COUNT
                    mAvrVal = 0F
                    mAvrStart = true
                    mCalState = 3
                }
            })
        mCalBut = findViewById<android.view.View?>(R.id.buttonCal) as android.widget.Button?
        mCalBut?.setOnClickListener(
            object : View.OnClickListener {
            override fun onClick(v: View) { // Code here executes on main thread after user presses button
                mAvrCnt = AVERAGE_COUNT
                mAvrVal = 0F
                mAvrStart = true
                if (mCalState != 2)
                {
                    mMessageLabel?.setText(String.format("Wait! Calibrating"))

                    mCalState = 1
                }
            }
        })
    }

    private val mScanCallback: ScanCallback = object : ScanCallback() {
        override fun onScanResult(
            callbackType: Int,
            result: ScanResult
        ) {
            Log.i("callbackType", callbackType.toString())
            Log.i("result", result.toString())
            val device = result.device
            val scanRecord = result.scanRecord
            val scanData = scanRecord!!.bytes
            val name = scanRecord.deviceName
            val deviceID: Long = 0
            val manuf = scanRecord.getManufacturerSpecificData(0x0177)

            if (manuf == null)// || manuf.size < 4)
                return

            when (manuf[0]) {
                BLEADV_MANDATA_TYPE_ADC -> {
                    val v = ByteBuffer.wrap(
                        manuf,
                        5,
                        4
                    ).order(ByteOrder.LITTLE_ENDIAN).float
                    var w = (v - mZeroVal) * mScaleFactor
                    if ((mWeight - w).absoluteValue > 20F)
                    {
                        mWeight = w
                    }
                    else
                    {
                        mWeight = (mWeight + w) /2F
                    }
                    var s = String.format("%.3f Kg", mWeight / 1000F)
                    mWeightLabel?.setText(s)
                    s = String.format("%d", v.toInt())
                    mAdcLabel?.setText(s)

                    if (mAvrStart == true) {
                        mAvrVal += v
                        mAvrCnt--
                        if (mAvrCnt <= 0)
                        {
                            when (mCalState)
                            {
                                1-> {
                                    mZeroVal = mAvrVal / AVERAGE_COUNT.toFloat()
                                    mAvrStart = false
                                    mCalState = 2
                                    s = String.format("%d", mZeroVal.toInt())
                                    mZeroLabel?.setText(s)
                                    mMessageLabel?.setText("Put 1Kg load on and press Calibrate")
                                }
                                2-> {
                                    mScaleFactor = 1000F / (mAvrVal / AVERAGE_COUNT.toFloat() - mZeroVal)// / 0.138F//1000000F
                                    mAvrStart = false
                                    mCalState = 0
                                    s = String.format("%d", mScaleFactor.toInt())
                                    mScaleLabel?.setText(s)
                                    mMessageLabel?.setText(" ")
                                    mWeight = 0F
                                }
                                3-> {
                                    mZeroVal = mAvrVal / AVERAGE_COUNT.toFloat()
                                    mAvrStart = false
                                    mCalState = 0
                                    s = String.format("%d", mZeroVal.toInt())
                                    mZeroLabel?.setText(s)
                                }
                            }
                        }
                    }
                }
                BLEADV_MANDATA_SN -> {
                }
            }
        }
    }
}