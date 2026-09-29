package itz.glace.deskhalo;
import android.app.Activity;
import android.os.Bundle;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.widget.*;
import android.view.View;
public class MainActivity extends Activity {
    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        if(BuildConfig.DEBUG && getIntent().getBooleanExtra("testConnect",false)) {
            startActivity(new Intent(this,VrActivity.class).putExtra("host",getIntent().getStringExtra("host")).putExtra("code",getIntent().getStringExtra("code")).putExtra("testFps",getIntent().getIntExtra("testFps",0)).putExtra("testResolution",getIntent().getIntExtra("testResolution",0))); finish(); return;
        }
        LinearLayout root=new LinearLayout(this); root.setOrientation(1); root.setPadding(52,32,52,28); root.setBackground(new GradientDrawable(GradientDrawable.Orientation.TL_BR,new int[]{0xff14161b,0xff1f2126,0xff2a2d34}));
        ImageView logo=new ImageView(this);logo.setImageResource(itz.glace.deskhalo.R.drawable.deskhalo_logo);root.addView(logo,new LinearLayout.LayoutParams(80,80));
        root.addView(label("DESKHALO",18,0xffffbd45)); root.addView(label("Your desk. More space.",34,Color.WHITE));
        root.addView(label("Start sharing in DeskHalo Host, then enter its address and pairing code.",16,0xffa6a9b2));
        EditText host=new EditText(this); host.setSingleLine(); host.setTextColor(Color.WHITE); host.setHintTextColor(0xff999999); host.setHint("PC IP address");
        host.setText("127.0.0.1"); host.setEnabled(false); root.addView(host);
        Switch usb=new Switch(this);usb.setText("USB connection (recommended)");usb.setChecked(true);root.addView(usb);usb.setOnCheckedChangeListener((button,checked)->{host.setEnabled(!checked);host.setText(checked?"127.0.0.1":getPreferences(0).getString("host",""));});
        EditText code=new EditText(this); code.setTextColor(Color.WHITE); code.setHintTextColor(0xff999999); code.setHint("Six-digit pairing code"); code.setInputType(2); root.addView(code);
        root.addView(label("USB: 127.0.0.1  •  Wi-Fi / hotspot: your PC's local IP\nLeft ≡ opens workspace controls. Passthrough starts automatically.",14,0xffa6a9b2));
        Button enter=new Button(this); enter.setText("ENTER YOUR WORKSPACE  →"); enter.setTextColor(0xff18130a);
        GradientDrawable bg=new GradientDrawable(); bg.setColor(0xffffbd45); bg.setCornerRadius(24); enter.setBackground(bg);
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,60); lp.topMargin=20; root.addView(enter,lp);
        enter.setOnClickListener(v->{
            String address=host.getText().toString().trim(), pin=code.getText().toString().trim();
            if(!address.matches("[a-zA-Z0-9.:-]+")||!pin.matches("[0-9]{6}")){Toast.makeText(this,"Enter an address and six-digit code",0).show();return;}
            if(!usb.isChecked())getPreferences(0).edit().putString("host",address).apply();
            startActivity(new Intent(this,VrActivity.class).putExtra("host",address).putExtra("code",pin));
        });
        ScrollView scroll=new ScrollView(this); scroll.addView(root); setContentView(scroll);
    }
    private TextView label(String value,int size,int color){TextView t=new TextView(this);t.setTypeface(Typeface.createFromAsset(getAssets(),"fonts/IBMPlexSans-ExtraLight.ttf"));t.setText(value);t.setTextSize(size);t.setTextColor(color);t.setPadding(0,8,0,12);return t;}
}
