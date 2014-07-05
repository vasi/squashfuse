/*
 * Copyright (c) 2014 Dave Vasilevsky <dave@vasilevsky.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "SQViewController.h"
#include "squashfuse.h"

@interface SQViewController ()

@end

@implementation SQViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    NSURL *path = [[NSBundle mainBundle]
                   URLForResource: @"test" withExtension: @"squashfs"];
    sqfs_input *input = malloc(sizeof(sqfs_input));
    sqfs_err err = sqfs_input_open(input, [path fileSystemRepresentation]);
    
    sqfs fs;
    err = sqfs_init(&fs, input);
    
    sqfs_traverse trv;
    err = sqfs_traverse_open(&trv, &fs, sqfs_inode_root(&fs));
    NSMutableString *str = [[NSMutableString alloc] init];
    while (sqfs_traverse_next(&trv, &err)) {
        if (trv.dir_end)
            continue;
        [str appendFormat: @"%s\n", sqfs_traverse_path(&trv)];
    }
    sqfs_destroy(&fs, true);
    
    self.textView.text = str;
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

@end
