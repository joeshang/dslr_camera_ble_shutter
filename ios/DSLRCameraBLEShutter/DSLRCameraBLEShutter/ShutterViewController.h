//
//  ShutterViewController.h
//  DSLRCameraBLEShutter
//
//  Created by Joe Shang on 10/24/14.
//  Copyright (c) 2014 Shang Chuanren. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <CoreBluetooth/CoreBluetooth.h>

@interface ShutterViewController : UIViewController
< CBCentralManagerDelegate, CBPeripheralDelegate, UIPickerViewDelegate, UIPickerViewDataSource >

@property (nonatomic, strong) CBCentralManager *centralManager;
@property (nonatomic, strong) CBPeripheral *peripheral;

@property (weak, nonatomic) IBOutlet UIButton *focusButton;
@property (weak, nonatomic) IBOutlet UIButton *shootingButton;
@property (weak, nonatomic) IBOutlet UISegmentedControl *parametersType;
@property (weak, nonatomic) IBOutlet UIPickerView *parametersPicker;
@property (weak, nonatomic) IBOutlet UITextField *targetCount;


@end
