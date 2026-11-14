"""Bounded data-only assembler for Sonic 1 68K SMPS music.

No includes, file reads, CPU instructions, user macros or external programs.
The native importer validates the resulting sequence before installing it.
"""
import ast
import operator
import re

OPS={ast.Add:operator.add,ast.Sub:operator.sub,ast.Mult:operator.mul,
     ast.FloorDiv:operator.floordiv,ast.LShift:operator.lshift,
     ast.RShift:operator.rshift,ast.BitOr:operator.or_,ast.BitAnd:operator.and_,ast.BitXor:operator.xor}

def assemble(text):
    if len(text)>1024*1024:
        raise ValueError('SMPS source exceeds 1 MiB')
    symbols={'nrst':0x80,'panleft':0x80,'panright':0x40,'pancenter':0xc0,'pannone':0,
             'dkick':0x81,'dsnare':0x82,'dclap':0x83,'dscratch':0x84,'dtimpani':0x85}
    for octave in range(8):
        for note,semitone in [('c',0),('cs',1),('df',1),('d',2),('ds',3),('ef',3),('e',4),('f',5),('fs',6),('gf',6),('g',7),('gs',8),('af',8),('a',9),('as',10),('bf',10),('b',11)]:
            value=0x81+octave*12+semitone
            if value<0xe0:symbols[f'n{note}{octave}']=value
    symbols.update({f'ftone_{n:02x}':n for n in range(14)})
    lines=[(i,line.split(';',1)[0].strip()) for i,line in enumerate(text.splitlines(),1)]
    if len(lines)>16384:raise ValueError('Too many SMPS source lines')
    for pass_number in range(3):
        output=bytearray();start=None;voice={}
        def expr(source):
            source=re.sub(r'\$([0-9a-fA-F]+)',r'0x\1',source.strip()).lower()
            source=re.sub(r'(?<![\w)])\*(?![\w(])',str(len(output)),source)
            tree=ast.parse(source,mode='eval')
            if sum(1 for _ in ast.walk(tree))>64:raise ValueError('Expression is too complex')
            def value(node):
                if isinstance(node,ast.Constant) and type(node.value)is int:return node.value
                if isinstance(node,ast.Name):
                    if node.id in symbols:return symbols[node.id]
                    if pass_number<2:return 0
                    raise ValueError('Unknown symbol '+node.id)
                if isinstance(node,ast.UnaryOp) and isinstance(node.op,(ast.USub,ast.UAdd,ast.Invert)):
                    v=value(node.operand);return -v if isinstance(node.op,ast.USub)else ~v if isinstance(node.op,ast.Invert)else v
                if isinstance(node,ast.BinOp) and type(node.op)in OPS:
                    a,b=value(node.left),value(node.right)
                    if isinstance(node.op,(ast.LShift,ast.RShift))and not 0<=b<=31:raise ValueError('Invalid shift')
                    result=OPS[type(node.op)](a,b)
                    if abs(result)>0xffffffff:raise ValueError('Expression is out of range')
                    return result
                raise ValueError('Only integer SMPS data expressions are supported')
            return value(tree.body)
        def byte(v):
            if not -128<=v<=255:raise ValueError('Byte is out of range')
            output.append(v&255)
        def word(v):
            if not -32768<=v<=65535:raise ValueError('Word is out of range')
            output.extend((v&65535).to_bytes(2,'big'))
        for number,line in lines:
            if not line:continue
            try:
                label=re.match(r'^([\w.]+):\s*(.*)$',line)
                if label:
                    symbols[label[1].lower()]=len(output);line=label[2]
                    if start is None:start=len(output)
                    if not line:continue
                equ=re.match(r'^([\w.]+)\s+(?:equ|=)\s+(.+)$',line,re.I)
                if equ:symbols[equ[1].lower()]=expr(equ[2]);continue
                parts=line.split(None,1);op=parts[0].lower();args=[x.strip()for x in parts[1].split(',')]if len(parts)>1 else[]
                if op in ('cpu','org','even','end'):
                    if op=='even'and len(output)&1:byte(0)
                    elif op=='org'and expr(args[0])!=len(output):raise ValueError('SMPS data must start at zero and remain contiguous')
                    elif op=='cpu'and args!=['68000']:raise ValueError('Only 68000 data is supported')
                    continue
                if op in ('dc.b','dc.w'):
                    for arg in args:(byte if op=='dc.b'else word)(expr(arg))
                elif op=='smpsheaderstartsong':
                    if args and expr(args[0])!=1:raise ValueError('Use Sonic 1 68K SMPS source (driver version 1)')
                    start=len(output)
                elif op=='smpsheadervoice':word(expr(args[0])-(start or 0))
                elif op in ('smpsheaderchan','smpsheadertempo'):
                    if len(args)!=2:raise ValueError('Expected two header values')
                    for arg in args:byte(expr(arg))
                elif op in ('smpsheaderdac','smpsheaderfm','smpsheaderpsg'):
                    count=5 if op=='smpsheaderpsg'else 3
                    if len(args)>count:raise ValueError('Too many channel header values')
                    word(expr(args[0])-(start or 0))
                    for i in range(1,count):byte(expr(args[i])if i<len(args)else 0)
                elif op in ('smpsjump','smpscall','smpsloop'):
                    byte({'smpsjump':0xf6,'smpscall':0xf8,'smpsloop':0xf7}[op])
                    if op=='smpsloop':byte(expr(args[0]));byte(expr(args[1]))
                    word(expr(args[-1])-len(output)-1)
                elif op.startswith('smpsvc'):
                    key=op[6:];voice[key]=[expr(arg)for arg in args]
                    if key=='totallevel':
                        def four(name):
                            values=voice.get(name)
                            if values is None or len(values)!=4:raise ValueError('Voice needs four '+name+' values')
                            return list(reversed(values))
                        byte((voice.get('algorithm',[0])[0]&7)|((voice.get('feedback',[0])[0]&7)<<3))
                        for a,b in zip(four('detune'),four('coarsefreq')):byte(((a&7)<<4)|(b&15))
                        for a,b in zip(four('ratescale'),four('attackrate')):byte(((a&3)<<6)|(b&31))
                        for a,b in zip(four('ampmod'),four('decayrate1')):byte(((a&1)<<7)|(b&31))
                        for v in four('decayrate2'):byte(v&31)
                        for a,b in zip(four('decaylevel'),four('releaserate')):byte(((a&15)<<4)|(b&15))
                        for v in four('totallevel'):byte(v&127)
                        voice={}
                else:
                    commands={'smpspan':(0xe0,1),'smpsdetune':(0xe1,1),'smpsreturn':(0xe3,0),'smpsfade':(0xe4,0),
                              'smpsaltervol':(0xe6,1),'smpsnoattack':(0xe7,0),'smpsnotefill':(0xe8,1),
                              'smpsalterpitch':(0xe9,1),'smpssettempo':(0xea,1),'smpssettempodiv':(0xeb,1),
                              'smpspsgaltervol':(0xec,1),'smpssetvoice':(0xef,1),'smpsmodset':(0xf0,4),
                              'smpsmodon':(0xf1,0),'smpsstop':(0xf2,0),'smpspsgform':(0xf3,1),
                              'smpsmodoff':(0xf4,0),'smpspsgvoice':(0xf5,1)}
                    if op not in commands:raise ValueError('Unsupported data directive or SMPS macro '+op)
                    code,count=commands[op]
                    if len(args)!=count:raise ValueError('Incorrect SMPS macro argument count')
                    byte(code)
                    for arg in args:byte(expr(arg))
                if len(output)>=0x8000:raise ValueError('SMPS song must be smaller than 32 KiB')
            except (ValueError,SyntaxError,ZeroDivisionError,IndexError)as error:
                raise ValueError(f'SMPS line {number}: {error}')from error
    return bytes(output)
