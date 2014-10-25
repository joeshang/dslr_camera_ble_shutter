//
//  ShutterViewController.m
//  DSLRCameraBLEShutter
//
//  Created by Joe Shang on 10/24/14.
//  Copyright (c) 2014 Shang Chuanren. All rights reserved.
//

#import "ShutterViewController.h"

#define kBLEShutterServiceUUID              @"FFF0"
#define kBLEShutterFocusUUID                @"FFF1"
#define kBLEShutterShootingUUID             @"FFF2"
#define kBLEShutterStopUUID                 @"FFF3"
#define kBLEShutterProgressUUID             @"FFF4"

typedef struct
{
    int hour;
    int minute;
    int second;
}DCBSTimeUnit;

@interface ShutterViewController ()

@property (nonatomic, assign) DCBSTimeUnit delay;
@property (nonatomic, assign) DCBSTimeUnit interval;
@property (nonatomic, assign) DCBSTimeUnit exposure;

- (void)focus;
- (void)shootingWithCount:(int)count
                    delay:(int)delay
                 interval:(int)interval
                 exposure:(int)exposure;
- (void)stop;

@end

@implementation ShutterViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    self.focusButton.enabled = NO;
    self.shootingButton.enabled = NO;
    self.centralManager = [[CBCentralManager alloc] initWithDelegate:self queue:nil];
    [self.centralManager scanForPeripheralsWithServices:nil options:nil];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
}

- (void)handleDisconnect
{
    if (self.peripheral)
    {
        self.peripheral.delegate = nil;
        self.peripheral = nil;
    }
    self.focusButton.enabled = NO;
    self.shootingButton.enabled = NO;
}

#pragma mark - Target-action

- (IBAction)onFocusButtonClicked:(id)sender
{
    [self focus];
}

- (IBAction)onShootingButtonClicked:(id)sender
{
    int delay = self.delay.second + self.delay.minute * 60 + self.delay.hour * 3600;
    int interval = self.interval.second + self.interval.minute * 60 + self.interval.hour * 3600;
    int exposure = self.exposure.second + self.exposure.minute * 60 + self.exposure.hour * 3600;
    [self shootingWithCount:[self.targetCount.text intValue]
                      delay:delay * 1000
                   interval:interval * 1000
                   exposure:exposure * 1000];
}

- (IBAction)onParameterTypeChange:(id)sender
{
    int hour = 0;
    int minute = 0;
    int second = 0;
    switch (self.parametersType.selectedSegmentIndex)
    {
        case 0:
            hour = self.delay.hour;
            minute = self.delay.minute;
            second = self.delay.second;
            break;
        case 1:
            hour = self.interval.hour;
            minute = self.interval.minute;
            second = self.interval.second;
            break;
        case 2:
            hour = self.exposure.hour;
            minute = self.exposure.minute;
            second = self.exposure.second;
            break;
        default:
            break;
    }
    [self.parametersPicker selectRow:hour   inComponent:0 animated:YES];
    [self.parametersPicker selectRow:minute inComponent:1 animated:YES];
    [self.parametersPicker selectRow:second inComponent:2 animated:YES];
}

#pragma mark - UIPickerViewDataSource

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView *)pickerView
{
    return 3;
}

- (NSInteger)pickerView:(UIPickerView *)pickerView numberOfRowsInComponent:(NSInteger)component
{
    if (component == 0)
    {
        return 24;
    }
    else
    {
        return 60;
    }
}

#pragma mark - UIPickerViewDelegate

- (NSString *)pickerView:(UIPickerView *)pickerView titleForRow:(NSInteger)row forComponent:(NSInteger)component
{
    NSString *unit = nil;
    switch (component) {
        case 0:
            unit = @"时";
            break;
        case 1:
            unit = @"分";
            break;
        case 2:
            unit = @"秒";
            break;
        default:
            break;
    }
    return [NSString stringWithFormat:@"%d %@", row, unit];
}

- (void)pickerView:(UIPickerView *)pickerView didSelectRow:(NSInteger)row inComponent:(NSInteger)component
{
    DCBSTimeUnit *time = nil;
    
    switch (self.parametersType.selectedSegmentIndex)
    {
        case 0:
            time = &_delay;
            break;
        case 1:
            time = &_interval;
            break;
        case 2:
            time = &_exposure;
            break;
        default:
            break;
    }
    
    if (time)
    {
        switch (component) {
            case 0:
                time->hour = row;
                break;
            case 1:
                time->minute = row;
                break;
            case 2:
                time->second = row;
                break;
            default:
                break;
        }
    }
}

#pragma mark - BLE Shutter Service

- (void)focus
{
    if (self.peripheral)
    {
        u_int8_t focus = 1;
        [self writeValue:[NSData dataWithBytes:&focus length:1]
                 forUUID:[CBUUID UUIDWithString:kBLEShutterFocusUUID]];
    }
}

- (void)shootingWithCount:(int)count
                    delay:(int)delay
                 interval:(int)interval
                 exposure:(int)exposure
{
    if (self.peripheral)
    {
        u_int8_t command[15];
        int index = 0;
        memcpy(command + index, &count, 2);
        index += 2;
        memcpy(command + index, &delay, 4);
        index += 4;
        memcpy(command + index, &exposure, 4);
        index += 4;
        memcpy(command + index, &interval, 4);
        index += 4;
        command[index] = 0;
        
        [self writeValue:[NSData dataWithBytes:command length:15]
                 forUUID:[CBUUID UUIDWithString:kBLEShutterShootingUUID]];
    }
}

- (void)stop
{
    if (self.peripheral)
    {
        u_int8_t stop = 1;
        [self writeValue:[NSData dataWithBytes:&stop length:1]
                 forUUID:[CBUUID UUIDWithString:kBLEShutterStopUUID]];
    }
}

- (void)writeValue:(NSData *)data forUUID:(CBUUID *)uuid
{
    if (!self.peripheral)
    {
        return;
    }
    
    CBCharacteristic *target = nil;
    for (CBService *service in self.peripheral.services)
    {
        if ([service.UUID isEqual:[CBUUID UUIDWithString:kBLEShutterServiceUUID]])
        {
            for (CBCharacteristic *characteristic in service.characteristics)
            {
                if ([characteristic.UUID isEqual:uuid])
                {
                    target = characteristic;
                    break;
                }
            }
        }
    }
    
    if (target)
    {
        if(target.properties & CBCharacteristicPropertyWriteWithoutResponse)
        {
            [self.peripheral writeValue:data
                      forCharacteristic:target
                                   type:CBCharacteristicWriteWithoutResponse];
        }
        else
        {
            [self.peripheral writeValue:data
                      forCharacteristic:target
                                   type:CBCharacteristicWriteWithResponse];
        }
    }
}

#pragma mark - CBCentralManager delegate

// Invoke when central manager's state is updated.
- (void)centralManagerDidUpdateState:(CBCentralManager *)central
{
    NSString *state = nil;
    
    switch ([central state])
    {
        case CBCentralManagerStateUnsupported:
            state = @"The hardware doesn't support Bluetooth Low Energy";
            break;
        case CBCentralManagerStateUnauthorized:
            state = @"The app is not authorized to use Bluetooth Low Energy";
            break;
        case CBCentralManagerStatePoweredOff:
            state = @"Bluetooth is current power off";
            [self handleDisconnect];
            break;
        case CBCentralManagerStateUnknown:
            state = @"Unknown Bluetooth state";
            break;
        case CBCentralManagerStateResetting:
            state = @"Resetting Bluetooth state";
            break;
        case CBCentralManagerStatePoweredOn:
            [self.centralManager scanForPeripheralsWithServices:nil options:nil];
            break;
        default:
            break;
    }
    
    if (state)
    {
        NSLog(@"central manager state: %@", state);
    }
}

// Invoke when central manager discover a peripheral when scanning.
- (void)centralManager:(CBCentralManager *)manager
 didDiscoverPeripheral:(CBPeripheral *)peripheral
     advertisementData:(NSDictionary *)advertisementData
                  RSSI:(NSNumber *)RSSI // Received Signal Strength Indicator
{
    NSLog(@"discover peripheral");
    if ([peripheral.name isEqualToString:@"DSLR BLE Shutter"])
    {
        [self.centralManager stopScan];
        
        if (self.peripheral != peripheral)
        {
            self.peripheral = peripheral;
            
            [self.centralManager connectPeripheral:self.peripheral options:nil];
        }
    }
}

- (void)centralManager:(CBCentralManager *)central
  didConnectPeripheral:(CBPeripheral *)peripheral
{
    NSLog(@"connect to peripheral %@", peripheral);
    
    self.peripheral.delegate = self;
    [self.peripheral discoverServices:nil];
}

- (void)centralManager:(CBCentralManager *)central
didFailToConnectPeripheral:(CBPeripheral *)peripheral
                 error:(NSError *)error
{
    NSLog(@"failed to connect peripheral %@ with error: %@", peripheral, error);
    
    [self handleDisconnect];
}

- (void)centralManager:(CBCentralManager *)central
didDisconnectPeripheral:(CBPeripheral *)peripheral
                 error:(NSError *)error
{
    NSLog(@"disconnect to peripheral %@ with error: %@", peripheral, error);
    
    [self handleDisconnect];
    [self.centralManager scanForPeripheralsWithServices:nil options:nil];
}

#pragma mark - CBPeripheral delegate

- (void)peripheral:(CBPeripheral *)peripheral
didDiscoverServices:(NSError *)error
{
    for (CBService *service in peripheral.services)
    {
        if ([service.UUID isEqual:[CBUUID UUIDWithString:kBLEShutterServiceUUID]])
        {
            [peripheral discoverCharacteristics:nil forService:service];
        }
    }
}

- (void)peripheral:(CBPeripheral *)peripheral
didDiscoverCharacteristicsForService:(CBService *)service
             error:(NSError *)error
{
    if ([service.UUID isEqual:[CBUUID UUIDWithString:kBLEShutterServiceUUID]])
    {
        for (CBCharacteristic *characteristic in service.characteristics)
        {
            if ([characteristic.UUID isEqual:[CBUUID UUIDWithString:kBLEShutterProgressUUID]])
            {
                [peripheral setNotifyValue:YES forCharacteristic:characteristic];
            }
        }
        
        self.shootingButton.enabled = YES;
        self.focusButton.enabled = YES;
    }
}

- (void)peripheral:(CBPeripheral *)peripheral
didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
             error:(NSError *)error
{
    if (!error)
    {
        if ([characteristic.UUID isEqual:[CBUUID UUIDWithString:kBLEShutterProgressUUID]])
        {
            const u_int8_t *data = [characteristic.value bytes];
            u_int16_t progress = CFSwapInt16LittleToHost(*(u_int16_t *)data);
            
            NSLog(@"progress = %d", progress);
        }
    }
    else
    {
        NSLog(@"update characteristic with error: %@", error);
    }
}
    
@end
