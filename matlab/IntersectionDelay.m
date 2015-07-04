
clear all;
close all;
clc;    % position the cursor at the top of the screen
%clf;   % closes the figure window

% ---------------------------------------------------------------

% total number of simulation runs
runTotal = 3;

% path to folder
basePATH = '../results/cmd/full_fix_web_adap_balanced';

for runNumber = 0:runTotal-1

fprintf('\n>>> runNumber %d:\n', runNumber);
    
% clear variables at the begining of each run
clearvars -except runNumber runTotal basePATH

% ----------------------------------------------------------------

disp('calculating average queue size ...');

path2 = sprintf('%s/%d_TLqueuingData.txt', basePATH, runNumber);
file_id = fopen(path2);
formatSpec = '%d %f %s %d %d';
C_text = textscan(file_id, formatSpec, 'HeaderLines', 3);
fclose(file_id);

indices = C_text{1,1};
timeSteps = C_text{1,2};
totalQs = C_text{1,4};
laneCounts = C_text{1,5};

% average queue size in each time step
averageQueueSize_all = double(totalQs) ./ double(laneCounts);

% aggregate every 300 values together
aggregateInterval_queue = 300;

rows = size(averageQueueSize_all, 1);
index = 1;
for i=1 : aggregateInterval_queue : rows-aggregateInterval_queue
    
    startIndex = i;
    endIndex = startIndex + aggregateInterval_queue - 1;
    
    averageQueueSize(index) = double(sum(averageQueueSize_all(startIndex:endIndex))) / double(aggregateInterval_queue);  
    
    middleIndex = floor( double((startIndex + endIndex)) / 2. );
    timeSteps_Q(index) = timeSteps(middleIndex);
    
    index = index + 1;
    
end

% -----------------------------------------------------------------

disp('reading vehicleDelay.txt ...');

path = sprintf('%s/%d_vehicleDelay.txt', basePATH, runNumber);
file_id = fopen(path);
formatSpec = '%s %s %s %f %d %f %f %f %f %f %f';
C_text = textscan(file_id, formatSpec, 'HeaderLines', 2);
fclose(file_id);

vehicles = C_text{1,1};
entrance = C_text{1,4};
crossed = C_text{1,5};
startDecel = C_text{1,7};
startStopping = C_text{1,8};
crossedTime = C_text{1,9};
startAccel = C_text{1,10};
endDelay = C_text{1,11};

% stores vehicle IDs
vIDs = unique(vehicles);
vIDs = sort_nat(vIDs);
VehNumbers = size(vIDs,1);

[rows,~] = size(vehicles);

% preallocating and initialization with -1
indexTS = zeros(7,rows) - 1;

for i=1:rows 
    
    % get the current vehicle name
    vehicle = char(vehicles(i,1));
        
    vNumber = find(ismember(vIDs,vehicle)); 

    indexTS(1,vNumber) = double(entrance(i,1));       
    indexTS(2,vNumber) = double(startDecel(i,1));
    indexTS(3,vNumber) = double(startStopping(i,1));
    indexTS(4,vNumber) = double(crossedTime(i,1));
    indexTS(5,vNumber) = double(startAccel(i,1));
    indexTS(6,vNumber) = double(endDelay(i,1));
    
    if(double(startAccel(i,1)) == -1 && double(startDecel(i,1)) ~= -1)
        error('startAccel can not be -1');
    end
    
    if(crossed(i) == 1 && double(crossedTime(i,1)) == -1)
        error('crossedTime can not be -1');
    end

end

fprintf( 'totalVeh: %d, crossed: %d, noSlowDown: %d, noWaiting: %d \n', VehNumbers, sum(crossed(:,1) == 1), sum(indexTS(2,:) == -1), sum(indexTS(3,:) == -1) );

% ---------------------------------------------------------------

% making a cell array that contains the complete view of the system

if(false)
    
    disp('making uitable ...');
    
    cell1 = vIDs;
    cell2 = num2cell(crossed(:,1));
    cell3 = [ num2cell(indexTS(1,:)') num2cell(indexTS(2,:)') num2cell(indexTS(3,:)') num2cell(indexTS(4,:)') num2cell(indexTS(5,:)') num2cell(indexTS(6,:)') num2cell(indexTS(7,:)')];

    data = [ cell1 cell2 cell3 ]';

    f = figure();

    t = uitable('Parent', f, 'Units', 'normalized', 'Position', [0.08 0.6 0.85 0.3], 'Data', data );
    t.Data = data;
    t.RowName = {'Veh ID', 'crossed?', 'entrance', 'deccel start', 'stopping start', 'cross time', 'accel start', 'end delay', 'YorR'}; 

end

% ---------------------------------------------------------------------

disp('calculating intersection delay ...');

interval_delay = 200;

rows = size(timeSteps, 1);
index = 1;
for j=1 : interval_delay-1 : rows-interval_delay
    
   startIndex = j;
   endIndex = startIndex + interval_delay - 1;
    
   startT = timeSteps(startIndex);
   endT = timeSteps(endIndex);
    
   delayedVehCount = 0;
   nonDelayedVehCount = 0;
   totalDelay = 0;

   for i=1:VehNumbers
       
       if(crossed(i) == 0)
           % this vehicle does not cross the intersection at all
           % We ignore this vehicle
           
       elseif(indexTS(1,i) > endT)
           % veh entrance is after this interval
           % We ignore this vehicle   
           
       elseif(indexTS(2,i) == -1)
               % this vehicle does not slow down because of getting r or Y.
               % In other words it is getting G or g and has right of way.
               % NOTE: in SUMO a vehicle slows down on g, but this slowing down is not due to
               % red or yellow light thus, indexTS(2,i) is still -1 
               
               % we need to check if the vehicle crossess the intersection at this interval
               
               if(indexTS(4,i) >= startT && indexTS(4,i) < endT)
                   % this vehicle crosses the intersection at this interval
                   nonDelayedVehCount = nonDelayedVehCount + 1;
               end
           
       elseif(indexTS(2,i) ~= -1)
           % this vehicle slows down due to getting a r or y signal
           % we need to check if this slowing down happens in this period
           % or has happend in a previous interval.
           % NOTE: if slowing down happens after this interval we don't care
               
               if(indexTS(2,i) >= startT && indexTS(2,i) < endT)
                   % slowing down occurs in this interval           
                   delayedVehCount = delayedVehCount + 1;
                   startDelay = indexTS(2,i);
                   endDelay = min( endT, indexTS(5,i) );
                   totalDelay = totalDelay + (endDelay - startDelay);   
           
               elseif(indexTS(2,i) < startT)
                   % slowing down occured in a previous interval                  
                   delayedVehCount = delayedVehCount + 1;
                   startDelay = indexTS(2,i);
                   endDelay = min( endT, indexTS(5,i) );
                   totalDelay = totalDelay + (endDelay - startDelay);
               end
       end
   end

   totalVehCount = nonDelayedVehCount + delayedVehCount;
   if(totalVehCount ~= 0)
       delay(index) = totalDelay / totalVehCount;
   else
       delay(index) = 0;
   end
   
   middleIndex = floor( double((startIndex + endIndex)) / 2. );
   timeSteps_D(index) = timeSteps(middleIndex);
    
   index = index + 1;

end
   
% -----------------------------------------------------------------

disp('calculating throughput ...');

interval_throughput = 900;

rows = size(timeSteps, 1);
index = 1;
for i=1 : interval_throughput-1 : rows-interval_throughput
    
    startIndex = i;
    endIndex = startIndex + interval_throughput - 1;
    
    startT = timeSteps(startIndex);
    endT = timeSteps(endIndex);
    
    vehCounter = 0;

    for j=1:VehNumbers       
        if( crossed(j) == 1 && indexTS(4,j) >= startT && indexTS(4,j) < endT )
            vehCounter = vehCounter + 1;
        end       
    end 
    
    throughput(index) = vehCounter;
    
    middleIndex = floor( double((startIndex + endIndex)) / 2. );
    timeSteps_T(index) = timeSteps(middleIndex);
    
    index = index + 1;
    
end

% -----------------------------------------------------------------

% now we make a single plot for all the scenarios
disp('plotting ...');

if(runNumber == 0)
    figure('name', 'Speed', 'units', 'normalized', 'outerposition', [0 0 1 1]);
end

subplot(3,1,1);
plot(timeSteps_Q, averageQueueSize, 'LineWidth', 3);

% set font size
set(gca, 'FontSize', 17);

xlabel('Time (s)', 'FontSize', 17);
ylabel('Average Queue Size', 'FontSize', 17);

grid on;
hold on;
    
subplot(3,1,2);
plot(timeSteps_D, delay, 'LineWidth', 3);

% set font size
set(gca, 'FontSize', 17);

xlabel('Time (s)', 'FontSize', 17);
ylabel('Average Delay (s)', 'FontSize', 17);

grid on;
hold on;

subplot(3,1,3);
plot(timeSteps_T, throughput, 'LineWidth', 3);

% set font size
set(gca, 'FontSize', 17);

xlabel('Time (s)', 'FontSize', 17);
ylabel('Throughput', 'FontSize', 17);

grid on;
hold on;

% at the end of the last iteration
if(runNumber == runTotal-1)       

    for g=1:3
        subplot(3,1,g);
        Xlimit = get(gca,'xlim');
        set(gca, 'xtick' , 0:300:Xlimit(2)); 

        legend('fix-time' , 'adaptive Webster', 'traffic-actuated');
    end   
    
end

end

